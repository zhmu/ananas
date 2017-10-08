#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/trace.h"
#include "hda.h"

TRACE_SETUP;

namespace Ananas {
namespace HDA {

/*
 * Given audio group 'afg' on device 'dev', this will route 'channels'-channel
 * output to output 'o'. On success, returns the kmalloc()'ed routing plan.
 */
errorcode_t
HDADevice::RouteOutput(AFG& afg, int channels, Output& o, RoutingPlan** rp)
{
	HDA_VPRINTF("routing %d channel(s) to %d-channel output", channels, o.o_channels);

	/*
	 * We walk through the pins for this output in order; a stereo-pin can handle
	 * 2 channels, otherwise we'll deduct 1 channel as we go along. This ensures
	 * we'll use the speakers in the intended order.

	 * By routing an output, we need to determine the following path:
	 *
	 * [audio output] -> ... -> [output pin]
	 *
	 * We'll walk backwards from the pin(s), determining the possible set of
	 * components to use.
	 *
	 */

	/*
 	 * See how many nodes this AFG has; this is the maximum amount of nodes
	 * we need to route.
	 */
	int max_nodes = 0;
	LIST_FOREACH(&afg.afg_nodes, aw, Node_AW) {
		max_nodes++;
	}
	*rp = new RoutingPlan;
	(*rp)->rp_node = new Node_AW*[max_nodes];
	(*rp)->rp_num_nodes = 0;

	/*
	 * Step 1: Determine the pins that need to be routed; this can be less than
	 *         the total because, for example, things like 2.0 may be desired
	 *         over a 7.1 output group.
	 *
	 * On completion, route_node[0 .. route_node_cur] will a sorted list of nodes
	 * which we must consider routing.
	 */
	int channels_left = channels;
	for (int n = 0; channels_left > 0 && n < o.o_pingroup->pg_count; n++) {
		Node_Pin* output_pin = o.o_pingroup->pg_pin[n];
		int ch_count = HDA_PARAM_AW_CAPS_CHANCOUNTLSB(output_pin->aw_caps) + 1; /* XXX ext */
		channels_left -= ch_count;

		/*
		 * Okay, we need to route to this output; make a list of everything that is
		 * connected to here - we care only about mixers and outputs XXX we should
		 * consider selectors too
		 */
		HDA_VPRINTF("will use %d-channel output %s-%s", ch_count, 
		 ResolveLocationToString(HDA_CODEC_CFGDEFAULT_LOCATION(output_pin->p_cfg)),
		 PinColorString[HDA_CODEC_CFGDEFAULT_COLOR(output_pin->p_cfg)]);
		if (output_pin->aw_num_conn == 0) {
			delete *rp;
			Printf("unable to route from cad %d nid %d; it has no outputs",
			 output_pin->n_address.na_cad, output_pin->n_address.na_nid);
			return ANANAS_ERROR(NO_SPACE);
		}

		/*
		 * Walk through this pin's input list and start routing the very first item we see that
		 * is acceptable.
		 */
		errorcode_t err = ANANAS_ERROR(NO_SPACE);
		for(int pin_in_idx  = 0; pin_in_idx < output_pin->aw_num_conn; pin_in_idx++) {
			/* pin_in_aw is the node connected to output_pin's input index pin_in_idx */
			Node_AW* pin_in_aw = output_pin->aw_conn[pin_in_idx];
			
			/* First see if we have already routed this item */
			int found = 0;
			for (int i = 0; !found && i < (*rp)->rp_num_nodes; i++)
				found = (*rp)->rp_node[i] == pin_in_aw;
			if (found)
				continue;

			/* Enable output on this pin */
			uint32_t r;
			err = hdaFunctions->IssueVerb(HDA_MAKE_VERB_NODE(*output_pin, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_SETPINCONTROL, 
			 HDA_CODEC_PINCONTROL_HENABLE | HDA_CODEC_PINCONTROL_OUTENABLE
			)), &r, NULL);
			if (ananas_is_failure(err))
				break;

			/* Set input to what we found that should work */
			err = hdaFunctions->IssueVerb(HDA_MAKE_VERB_NODE(*output_pin, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_SETCONNSELECT, pin_in_idx)), &r, NULL);
			if (ananas_is_failure(err))
				break;

			/* Set output gain XXX We should check if the pin supports this */
			err = hdaFunctions->IssueVerb(HDA_MAKE_VERB_NODE(*output_pin, HDA_MAKE_PAYLOAD_ID4(HDA_CODEC_CMD_SET_AMP_GAINMUTE,
			 HDA_CODEC_AMP_GAINMUTE_OUTPUT | HDA_CODEC_AMP_GAINMUTE_LEFT | HDA_CODEC_AMP_GAINMUTE_RIGHT | HDA_CODEC_AMP_GAINMUTE_GAIN(60)
			)), &r, NULL);
			if (ananas_is_failure(err))
				break;

			/*
			 * Pin is routed to use input pin_in_aw; we must now configure it and
			 * whatever hooks to it.
			 */
			Node_AW* cur_aw = pin_in_aw;
			while(cur_aw != nullptr && ananas_is_success(err)) {
				/* Hook the input up to the list */
				(*rp)->rp_node[(*rp)->rp_num_nodes++] = cur_aw;
				switch(cur_aw->GetType()) {
					case NT_AudioOut:
						/*
						 * We've reached an 'audio out'; this is the end of the line as we
						 * can feed our audio output into.
						 */
						HDA_VPRINTF("routed audio output nid 0x%x to pin 0x%x", cur_aw->n_address.na_nid, output_pin->n_address.na_nid);

						/* Nothing to route anymore */
						cur_aw = nullptr;
						break;
					case NT_AudioMixer: {
						/*
						 * Audio mixers take >1 inputs and a single output; We'll repeat
						 * the trick to locate a sensible input.
						 *
						 * XXX This may be a bit too short-circuit; basically, this expects
						 *     audio mixers to have at least a single audio output
						 *     connected to them.
					 	 */
						Node_AW* mixer_in = NULL;
						for (int m = 0; m < cur_aw->aw_num_conn; m++) {
							Node_AW* aw = cur_aw->aw_conn[m];
							if(aw->GetType() != NT_AudioOut)
								continue;

							/* Found an audio output in 'aw'; see if it is already in use */
							bool found = false;
							for (int i = 0; !found && i < (*rp)->rp_num_nodes; i++)
								found = aw == (*rp)->rp_node[i];
							if(found)
								continue; /* Already routed; we need another one */

							/* Okay, this mixer connects to an unused audio output; set up the mixer first */

							/* Set input amplifiers - allow index 'm' */
							err = hdaFunctions->IssueVerb(HDA_MAKE_VERB_NODE(*aw, HDA_MAKE_PAYLOAD_ID4(HDA_CODEC_CMD_SET_AMP_GAINMUTE,
							 HDA_CODEC_AMP_GAINMUTE_INPUT | HDA_CODEC_AMP_GAINMUTE_LEFT | HDA_CODEC_AMP_GAINMUTE_RIGHT | HDA_CODEC_AMP_GAINMUTE_GAIN(60) |
							 HDA_CODEC_AMP_GAINMUTE_INDEX(m)
							)), &r, NULL);
							/* Loop below checks err for us */

							/* Set input amplifiers - mute everything not 'm' */
							for (int i = 0; ananas_is_success(err) && i < aw->aw_num_conn; i++)
								if (i != m)
									err = hdaFunctions->IssueVerb(HDA_MAKE_VERB_NODE(*aw, HDA_MAKE_PAYLOAD_ID4(HDA_CODEC_CMD_SET_AMP_GAINMUTE,
									 HDA_CODEC_AMP_GAINMUTE_INPUT | HDA_CODEC_AMP_GAINMUTE_LEFT | HDA_CODEC_AMP_GAINMUTE_RIGHT | HDA_CODEC_AMP_GAINMUTE_MUTE |
									 HDA_CODEC_AMP_GAINMUTE_INDEX(i)
									)), &r, NULL);
							if (ananas_is_failure(err))
								break;
							
							/* Set output gain */
							err = hdaFunctions->IssueVerb(HDA_MAKE_VERB_NODE(*aw, HDA_MAKE_PAYLOAD_ID4(HDA_CODEC_CMD_SET_AMP_GAINMUTE,
							 HDA_CODEC_AMP_GAINMUTE_OUTPUT | HDA_CODEC_AMP_GAINMUTE_LEFT | HDA_CODEC_AMP_GAINMUTE_RIGHT | HDA_CODEC_AMP_GAINMUTE_GAIN(60)
							)), &r, NULL);
							if (ananas_is_failure(err))
								break;

							/* We've set up this mixer correctly */
							mixer_in = aw;
							break;
						}
						if (mixer_in == NULL) {
							Printf("TODO: when routing pin nid 0x%x, ended up at audio mixer nid 0x%x which we can't route to an audioout - aborting",
							 output_pin->n_address.na_nid, cur_aw->n_address.na_nid);
							err = ANANAS_ERROR(NO_SPACE);
						}

						/* Mixer was routed; we need to continue routing */
						cur_aw = mixer_in;
						break;
					}
					default:
						Printf("TODO: when routing pin nid 0x%x, ended up at unsupported node nid 0x%x type 0x%d - aborting",
						 output_pin->n_address.na_nid, cur_aw->n_address.na_nid, cur_aw->GetType());
						err = ANANAS_ERROR(NO_SPACE);
						break;
				}
			}

			/*
			 * If we are here, either err isn't happy or we've run out of pins to
			 * route and have completed this pin's path.
			 */
			break;
		}

		if (ananas_is_failure(err)) {
			kfree(*rp);
			return err;
		}
	}

#if HDA_VERBOSE
	kprintf("routed %d channel(s) to %d-channel output ->", channels, o.o_channels);
	for (int i = 0; i < (*rp)->rp_num_nodes; i++)
		kprintf(" %x", (*rp)->rp_node[i]->n_address.na_nid);
	kprintf("\n");
#endif

	return ananas_success();
}

} // namespace HDA
} // namespace Ananas

/* vim:set ts=2 sw=2: */
