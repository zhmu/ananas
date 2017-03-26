#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/trace.h>
#include "hda.h"

TRACE_SETUP;

namespace Ananas {
namespace HDA {

#if HDA_VERBOSE
const char* const PinConnectivityString[] = { "jack", "none", "fixed", "both" };
const char* const PinDefaultDeviceString[] = {
 "line-out", "speaker", "hp-out", "cd", "spdif-out", "dig-other-out",
 "modem-line-side", "modem-handset-side", "line-in", "aux", "mic-in",
 "telephony", "spdif-in", "digital-other-in", "reserved", "other" };
const char* const PinConnectionTypeString[] = {
 "unknown", "1/8\"", "1/4\"", "atapi", "rca", "optical", "other-digital",
 "other-analog", "din", "xlr", "rj-11", "combination", "?c", "?d", "?e", "other" };
const char* const PinColorString[] = {
 "unknown", "black", "grey", "blue", "green", "red", "orange", "yellow",
 "purple", "pink", "?a", "?b", "?c", "?d", "white", "other"
};
#endif

errorcode_t
HDADevice::FillAWConnectionList(Node_AW& aw)
{
	if (!HDA_PARAM_AW_CAPS_CONNLIST(aw.aw_caps))
		return ananas_success();

	uint32_t r;
	errorcode_t err = hdaFunctions->IssueVerb(HDA_MAKE_VERB_NODE(aw, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_CONLISTLEN)), &r, NULL);
	ANANAS_ERROR_RETURN(err);
	int cl_len = HDA_CODEC_PARAM_CONLISTLEN_LENGTH(r);
	if (HDA_CODEC_PARAM_CONLISTLEN_LONGFORM(r)) {
		Printf("unsupported long nentry form!");
		return ANANAS_ERROR(NO_DEVICE);
	}


	aw.aw_num_conn = cl_len;
	aw.aw_conn = new Node_AW*[cl_len];

	for (int idx = 0; idx < cl_len; idx += 4) {
		err = hdaFunctions->IssueVerb(HDA_MAKE_VERB_NODE(aw, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETCONNLISTENTRY, idx)), &r, NULL);
		ANANAS_ERROR_RETURN(err);

		for (int n = 0; n < 4; n++) {
			if (n + idx == cl_len)
				break;
			uint8_t e = r & 0xff;
			r >>= 8;
			/*
			 * Kludge: we may encounter node id's we aren't yet ready for - for now,
			 * just store the nid in the list and fix up the pointers later.
			 */
			aw.aw_conn[idx + n] = (Node_AW*)(addr_t)e;
		}
	}

	return ananas_success();
}

errorcode_t
HDADevice::AttachWidget_AudioOut(Node_AudioOut& ao)
{
	errorcode_t err = hdaFunctions->IssueVerb(HDA_MAKE_VERB_NODE(ao, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_PCM)), &ao.ao_pcm, NULL);
	ANANAS_ERROR_RETURN(err);

	/* Use the AFG's default if the widget doesn't specify any */
	if (ao.ao_pcm == 0)
		ao.ao_pcm = hda_afg->afg_pcm;
	return ananas_success();
}

errorcode_t
HDADevice::AttachWidget_AudioIn(Node_AudioIn& ai)
{
	return ananas_success();
}

errorcode_t
HDADevice::AttachWidget_AudioMixer(Node_AudioMixer& am)
{
	int aw_caps = am.aw_caps;

	am.am_ampcap_in = 0;
	am.am_ampcap_out = 0;
	
	errorcode_t err;
	if (HDA_PARAM_AW_CAPS_INAMPPRESENT(aw_caps)) {
		err = hdaFunctions->IssueVerb(HDA_MAKE_VERB_NODE(am, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_AMPCAP_IN)), &am.am_ampcap_in, NULL);
		ANANAS_ERROR_RETURN(err);
	}

	if (HDA_PARAM_AW_CAPS_OUTAMPPRESENT(aw_caps)) {
		err = hdaFunctions->IssueVerb(HDA_MAKE_VERB_NODE(am, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_AMPCAP_OUT)), &am.am_ampcap_out, NULL);
		ANANAS_ERROR_RETURN(err);
	}

	err = FillAWConnectionList(am);
	ANANAS_ERROR_RETURN(err);
	return ananas_success();
}

errorcode_t
HDADevice::AttachWidget_AudioSelector(Node_AudioSelector& as)
{
	errorcode_t err;
	err = FillAWConnectionList(as);
	ANANAS_ERROR_RETURN(err);

	kprintf("hda_attach_widget_audioselector: TODO\n");

	return ananas_success();
}

errorcode_t
HDADevice::AttachWidget_VendorDefined(Node_VendorDefined& vd)
{
	return ananas_success();
}

errorcode_t
HDADevice::AttachWidget_PinComplex(Node_Pin& p)
{
	errorcode_t err = hdaFunctions->IssueVerb(HDA_MAKE_VERB_NODE(p, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_PINCAP)), &p.p_cap, NULL);
	ANANAS_ERROR_RETURN(err);
	
	err = hdaFunctions->IssueVerb(HDA_MAKE_VERB_NODE(p, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETCFGDEFAULT, 0)), &p.p_cfg, NULL);
	ANANAS_ERROR_RETURN(err);

	return FillAWConnectionList(p);
}

errorcode_t
HDADevice::AttachNode_AFG(AFG& afg)
{
	int cad = afg.afg_cad;
	int nid = afg.afg_nid;

	/* Fetch the default parameters; these are used if the subnodes don't supply them */
	uint32_t r;
	errorcode_t err = hdaFunctions->IssueVerb(HDA_MAKE_VERB(cad, nid, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_PCM)), &afg.afg_pcm, NULL);
	ANANAS_ERROR_RETURN(err);

	/* Again, audio subnodes have even more subnodes... query it here */
	err = hdaFunctions->IssueVerb(HDA_MAKE_VERB(cad, nid, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_SUBNODECOUNT)), &r, NULL);
	ANANAS_ERROR_RETURN(err);
	int sn_addr = HDA_PARAM_SUBNODECOUNT_START(r);
	int sn_total = HDA_PARAM_SUBNODECOUNT_TOTAL(r);
	//HDA_DEBUG("hda_attach_node_afg(): codec %d nodeid=%x -> sn_addr %x sn_total %d", cad, nodeid, sn_addr, sn_total);

	for (unsigned int sn = 0; sn < sn_total; sn++) {
		/* Obtain the audio widget capabilities of this node; this tells us what it is */
		int nid = sn_addr + sn;
		uint32_t aw_caps;
		err = hdaFunctions->IssueVerb(HDA_MAKE_VERB(cad, nid, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_AW_CAPS)), &aw_caps, NULL);
		ANANAS_ERROR_RETURN(err);

		Node_AW* aw = nullptr;
		NodeAddress nodeAddress(cad, nid);
		switch(HDA_PARAM_AW_CAPS_TYPE(aw_caps)) {
			case HDA_PARAM_AW_CAPS_TYPE_AUDIO_OUT:
				aw = new Node_AudioOut(nodeAddress, aw_caps);
				err = AttachWidget_AudioOut(static_cast<Node_AudioOut&>(*aw));
				break;
			case HDA_PARAM_AW_CAPS_TYPE_AUDIO_IN:
				aw = new Node_AudioIn(nodeAddress, aw_caps);
				err = AttachWidget_AudioIn(static_cast<Node_AudioIn&>(*aw));
				break;
			case HDA_PARAM_AW_CAPS_TYPE_AUDIO_MIXER:
				aw = new Node_AudioMixer(nodeAddress, aw_caps);
				err = AttachWidget_AudioMixer(static_cast<Node_AudioMixer&>(*aw));
				break;
			case HDA_PARAM_AW_CAPS_TYPE_AUDIO_SELECTOR:
				aw = new Node_AudioSelector(nodeAddress, aw_caps);
				err = AttachWidget_AudioSelector(static_cast<Node_AudioSelector&>(*aw));
				break;
			case HDA_PARAM_AW_CAPS_TYPE_AUDIO_PINCOMPLEX: {
				aw = new Node_Pin(nodeAddress, aw_caps);
				err = AttachWidget_PinComplex(static_cast<Node_Pin&>(*aw));
				break;
			}
			case HDA_PARAM_AW_CAPS_TYPE_AUDIO_VENDORDEFINED: {
				aw = new Node_VendorDefined(nodeAddress, aw_caps);
				err = AttachWidget_VendorDefined(static_cast<Node_VendorDefined&>(*aw));
				break;
			}
			default:
				Printf("ignored cad=%d nid=%d awtype=%d", cad, nid, HDA_PARAM_AW_CAPS_TYPE(aw_caps));
				break;
		}

		if (ananas_is_success(err) && aw != nullptr)
			LIST_APPEND(&afg.afg_nodes, aw);
#undef HDA_AWNODE_INIT
	}

	/*
	 * Okay, we are done - need to resolve the connection node ID's to pointers.
	 * This is O(N^2)... XXX
	 */
	LIST_FOREACH(&afg.afg_nodes, aw, Node_AW) {
		for(int n = 0; n < aw->aw_num_conn; n++) {
			uintptr_t nid = (uintptr_t)aw->aw_conn[n];
			Node_AW* aw_found = nullptr;
			LIST_FOREACH(&afg.afg_nodes, aw2, Node_AW) {
				if (aw2->n_address.na_nid != nid)
					continue;
				aw_found = aw2;
				break;
			}
			if (aw_found == nullptr) {
				Printf("cad %d nid %d lists connection nid %d, but not found - aborting",
				 aw->n_address.na_cad, aw->n_address.na_nid, nid);
				return ANANAS_ERROR(NO_DEVICE);
			}
			aw->aw_conn[n] = aw_found;
		}
	}

	return err;
}

#if HDA_VERBOSE
const char*
ResolveLocationToString(int location)
{
	switch((location >> 4) & 3) {
		case 0: /* external on primary chassis */
			switch(location & 0xf) {
				case 0: return "ext/n/a";
				case 1: return "ext/rear";
				case 2: return "ext/front";
				case 3: return "ext/left";
				case 4: return "ext/right";
				case 5: return "ext/top";
				case 6: return "ext/bottom";
				case 7: return "ext/rear";
				case 8: return "ext/drive-bay";
			}
			break;
		case 1: /* internal */
			switch(location & 0xf) {
				case 0: return "int/n/a";
				case 7: return "int/riser";
				case 8: return "int/display";
				case 9: return "int/atapi";
			}
			break;
		case 2: /* separate chassis */
			switch(location & 0xf) {
				case 0: return "sep/n/a";
				case 1: return "sep/rear";
				case 2: return "sep/front";
				case 3: return "sep/left";
				case 4: return "sep/right";
				case 5: return "sep/top";
				case 6: return "sep/bottom";
			}
			break;
		case 3: /* other */
			switch(location & 0xf) {
				case 0: return "oth/n/a";
				case 6: return "oth/bottom";
				case 7: return "oth/lid-inside";
				case 8: return "oth/lid-outside";
			}
			break;
	}
	return "?";
}

void
DumpAFG(AFG& afg)
{
	if (LIST_EMPTY(&afg.afg_nodes))
		return;

	static const char* aw_node_types[] = {
		"pc", "ao", "ai", "am", "as", "vd"
	};
	LIST_FOREACH(&afg.afg_nodes, aw, Node_AW) {
		kprintf("cad %x nid % 2x type %s ->", aw->n_address.na_cad, aw->n_address.na_nid, aw_node_types[aw->GetType()]);

		switch(aw->GetType()) {
			case NT_Pin: {
				auto& p = static_cast<Node_Pin&>(*aw);
				kprintf(" [");
				if (HDA_CODEC_PINCAP_HBR(p.p_cap))
					kprintf(" hbr");
				if (HDA_CODEC_PINCAP_DP(p.p_cap))
					kprintf(" dp");
				if (HDA_CODEC_PINCAP_EAPD(p.p_cap))
					kprintf(" eapd");
				if (HDA_CODEC_PINCAP_VREF_CONTROL(p.p_cap))
					kprintf(" vref-control");
				if (HDA_CODEC_PINCAP_HDMI(p.p_cap))
					kprintf(" hdmi");
				if (HDA_CODEC_PINCAP_BALANCED_IO(p.p_cap))
					kprintf(" balanced-io");
				if (HDA_CODEC_PINCAP_INPUT(p.p_cap))
					kprintf(" input");
				if (HDA_CODEC_PINCAP_OUTPUT(p.p_cap))
					kprintf(" output");
				if (HDA_CODEC_PINCAP_HEADPHONE(p.p_cap))
					kprintf(" headphone");
				if (HDA_CODEC_PINCAP_PRESENCE(p.p_cap))
					kprintf(" presence");
				if (HDA_CODEC_PINCAP_TRIGGER(p.p_cap))
					kprintf(" trigger");
				if (HDA_CODEC_PINCAP_IMPEDANCE(p.p_cap))
					kprintf(" impedance");

				kprintf(" ] %s %s %s (%s) %s default-ass/seq %d/%d",
				 ResolveLocationToString(HDA_CODEC_CFGDEFAULT_LOCATION(p.p_cfg)),
				 PinConnectionTypeString[HDA_CODEC_CFGDEFAULT_CONNECTION_TYPE(p.p_cfg)],
				 PinConnectivityString[HDA_CODEC_CFGDEFAULT_PORTCONNECTIVITY(p.p_cfg)],
				 PinColorString[HDA_CODEC_CFGDEFAULT_COLOR(p.p_cfg)],
				 PinDefaultDeviceString[HDA_CODEC_CFGDEFAULT_DEFAULTDEVICE(p.p_cfg)],
				 HDA_CODEC_CFGDEFAULT_DEFAULT_ASSOCIATION(p.p_cfg),
				 HDA_CODEC_CFGDEFAULT_SEQUENCE(p.p_cfg));
				break;
			}
			case NT_AudioOut: {
				auto& ao = static_cast<Node_AudioOut&>(*aw);

				kprintf(" formats [");
				if (HDA_CODEC_PARAM_PCM_B32(ao.ao_pcm))
					kprintf(" 32-bit");
				if (HDA_CODEC_PARAM_PCM_B24(ao.ao_pcm))
					kprintf(" 24-bit");
				if (HDA_CODEC_PARAM_PCM_B20(ao.ao_pcm))
					kprintf(" 20-bit");
				if (HDA_CODEC_PARAM_PCM_B16(ao.ao_pcm))
					kprintf(" 16-bit");
				if (HDA_CODEC_PARAM_PCM_B8(ao.ao_pcm))
					kprintf(" 8-bit");
				kprintf(" ] rate [");
				if (HDA_CODEC_PARAM_PCM_R_8_0(ao.ao_pcm))
					kprintf(" 8.0kHz");
				if (HDA_CODEC_PARAM_PCM_R_11_025(ao.ao_pcm))
					kprintf(" 11.025kHz");
				if (HDA_CODEC_PARAM_PCM_R_16_0(ao.ao_pcm))
					kprintf(" 16.0kHz");
				if (HDA_CODEC_PARAM_PCM_R_22_05(ao.ao_pcm))
					kprintf(" 22.05kHz");
				if (HDA_CODEC_PARAM_PCM_R_32_0(ao.ao_pcm))
					kprintf(" 32.0kHz");
				if (HDA_CODEC_PARAM_PCM_R_44_1(ao.ao_pcm))
					kprintf(" 44.1kHz");
				if (HDA_CODEC_PARAM_PCM_R_48_0(ao.ao_pcm))
					kprintf(" 48.0kHz");
				if (HDA_CODEC_PARAM_PCM_R_88_2(ao.ao_pcm))
					kprintf(" 88.2kHz");
				if (HDA_CODEC_PARAM_PCM_R_96_0(ao.ao_pcm))
					kprintf(" 96.0kHz");
				if (HDA_CODEC_PARAM_PCM_R_176_4(ao.ao_pcm))
					kprintf(" 172.4kHz");
				if (HDA_CODEC_PARAM_PCM_R_192_0(ao.ao_pcm))
					kprintf(" 192.0kHz");
				if (HDA_CODEC_PARAM_PCM_R_384_0(ao.ao_pcm))
					kprintf(" 384.0kHz");
				kprintf(" ]");

			}
			default:
				break;
		}

		if (aw->aw_num_conn > 0) {
			kprintf(" conn [");
			for(int n = 0; n < aw->aw_num_conn; n++) {
				kprintf("%s%x", (n > 0) ? "," : "", aw->aw_conn[n]->n_address.na_nid);
			}
			kprintf("]");
		}
		kprintf("\n");
	}

	LIST_FOREACH(&afg.afg_outputs, o, Output) {
		kprintf("output: %d-channel ->", o->o_channels);
		for (int n = 0; n < o->o_pingroup->pg_count; n++) {
			Node_Pin* p = o->o_pingroup->pg_pin[n];
			kprintf(" %s-%s",
			 ResolveLocationToString(HDA_CODEC_CFGDEFAULT_LOCATION(p->p_cfg)),
			 PinColorString[HDA_CODEC_CFGDEFAULT_COLOR(p->p_cfg)]);
		}
		kprintf("\n");
	}
}
#endif /* HDA_VERBOSE */

errorcode_t
HDADevice::AttachMultiPin_Render(AFG& afg, int association, int num_pins)
{
	auto pg = new PinGroup;
	pg->pg_pin = new Node_Pin*[num_pins];
	pg->pg_count = 0;

	/* Now locate each pin and put it into place */
	int num_channels = 0;
	for (unsigned int seq = 0; seq < 16; seq++) {
		Node_Pin* found_pin = nullptr;
		LIST_FOREACH(&afg.afg_nodes, aw, Node_AW) {
			if (aw->GetType() != NT_Pin)
				continue;
			auto& p = static_cast<Node_Pin&>(*aw);
			if (HDA_CODEC_CFGDEFAULT_DEFAULT_ASSOCIATION(p.p_cfg) != association)
				continue;
			if (HDA_CODEC_CFGDEFAULT_SEQUENCE(p.p_cfg) != seq)
				continue;
			found_pin = &p;

			int ch_count = HDA_PARAM_AW_CAPS_CHANCOUNTLSB(p.aw_caps) + 1; /* XXX ext */
			num_channels += ch_count;
			break;
		}

		if (found_pin != nullptr)
			pg->pg_pin[pg->pg_count++] = found_pin;
	}

	/* Make sure we have found all pins (this may be paranoid) */
	if (pg->pg_count != num_pins) {
		delete pg;
		return ANANAS_ERROR(NO_DEVICE);
	}

	/* Construct the output provider */
	auto o = new Output;
	o->o_channels = num_channels;
	o->o_pingroup = pg;
	o->o_pcm_output = 0;

	/*
	 * Let's see if we can actually route towards this entry; we use this to determine
	 * the maximum sample rate and such that we can use.
	 */
	RoutingPlan* rp;
	errorcode_t err = RouteOutput(afg, num_channels, *o, &rp);
	if (ananas_is_failure(err)) {
		delete pg;
		delete o;
		return err;
	}

	/*
	 * Routing worked; now, we'll walk through the routing plan and determine the
   * audio output's common capabilities.
	 *
	 * XXX I wonder if this is really required and whether just taking the first
	 * audio output wouldn't be sufficient.
	 */
	for (int n = 0; n < rp->rp_num_nodes; n++) {
		if (rp->rp_node[n]->GetType() != NT_AudioOut)
			continue;
		Node_AudioOut& ao = static_cast<Node_AudioOut&>(*rp->rp_node[n]);
		if (o->o_pcm_output == 0)
			o->o_pcm_output = ao.ao_pcm; /* nothing yet; use the first as base */
		else
			o->o_pcm_output &= ao.ao_pcm; /* combine what the next output has */
	}
	delete rp;

	LIST_APPEND(&afg.afg_outputs, o);
	return ananas_success();
}

errorcode_t
HDADevice::AttachSinglePin_Render(AFG& afg, int association)
{
	return AttachMultiPin_Render(afg, association, 1);
}

errorcode_t
HDADevice::AttachAFG(AFG& afg)
{
	if (LIST_EMPTY(&afg.afg_nodes))
		return ANANAS_ERROR(NO_DEVICE);

	/* Walk through all associations and see what we have got there */
	for (unsigned int association = 1; association < 15; association++) {
		int total_pins = 0;
		int num_input = 0, num_output = 0;
		LIST_FOREACH(&afg.afg_nodes, aw, Node_AW) {
			if (aw->GetType() != NT_Pin)
				continue;
			auto& p = static_cast<Node_Pin&>(*aw);
			if (HDA_CODEC_CFGDEFAULT_DEFAULT_ASSOCIATION(p.p_cfg) != association)
				continue;

			total_pins++;
			if (HDA_CODEC_PINCAP_INPUT(p.p_cap))
				num_input++;
			if (HDA_CODEC_PINCAP_OUTPUT(p.p_cap))
				num_output++;
		}

		/* Skip any association without any pins; those only clutter things up */
		if (total_pins == 0)
			continue;

		HDA_VPRINTF("association %d has %d pin(s), %d input(s), %d output(s)",
		 association, total_pins, num_input, num_output);

		if (total_pins == num_output) {
			if (total_pins > 1) {
				/* Multi-pin rendering device */
				errorcode_t err = AttachMultiPin_Render(afg, association, total_pins);
				ANANAS_ERROR_RETURN(err);
			} else /* total_pins == 1 */ {
				/* Single-pin rendering device */
				errorcode_t err = AttachSinglePin_Render(afg, association);
				ANANAS_ERROR_RETURN(err);
			}
		}

		if (total_pins == num_input) {
			if (total_pins > 1) {
				/* Multi-pin capture device TODO */
			} else /* total_pins == 1 */ {
				/* Single-pin capture device TODO */
			}
		}
	}

	return ananas_success();
}

errorcode_t
HDADevice::AttachNode(int cad, int nodeid)
{
	/*
	 * First step is to figure out the start address and subnode count; we'll
	 * need to dive into the subnodes.
	 */
	uint32_t r;
	errorcode_t err = hdaFunctions->IssueVerb(HDA_MAKE_VERB(cad, nodeid, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_SUBNODECOUNT)), &r, NULL);
	ANANAS_ERROR_RETURN(err);
	int sn_addr = HDA_PARAM_SUBNODECOUNT_START(r);
	int sn_total = HDA_PARAM_SUBNODECOUNT_TOTAL(r);

	/* Now dive deeper and see what the subnodes can do */
	hda_afg = NULL;
	for (unsigned int sn = 0; sn < sn_total; sn++) {
		int nid = sn_addr + sn;
		err = hdaFunctions->IssueVerb(HDA_MAKE_VERB(cad, nid, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_FGT)), &r, NULL);
		ANANAS_ERROR_RETURN(err);
		int fgt_type = HDA_PARAM_FGT_NODETYPE(r);
		if (fgt_type != HDA_PARAM_FGT_NODETYPE_AUDIO)
			continue; /* we don't care about non-audio types */
	
		if (hda_afg != NULL) {
			Printf("warning: ignoring afg %d (we only support one per device)", nid);
			continue;
		}

		/* Now dive into this subnode and attach it */
		auto afg = new AFG;
		afg->afg_cad = cad;
		afg->afg_nid = nid;
		LIST_INIT(&afg->afg_nodes);
		LIST_INIT(&afg->afg_outputs);
		hda_afg = afg;

		err = AttachNode_AFG(*afg);
		ANANAS_ERROR_RETURN(err);

		err = AttachAFG(*afg);
		ANANAS_ERROR_RETURN(err);
		hda_afg = afg;

#if HDA_VERBOSE
		DumpAFG(*afg);
#endif
		ANANAS_ERROR_RETURN(err);
	}

	return ananas_success();
}

} // namespace HDA
} // namespace Ananas

/* vim:set ts=2 sw=2: */
