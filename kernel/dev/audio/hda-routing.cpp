#include <ananas/types.h>
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "hda.h"
#include "verb.h"

TRACE_SETUP;

namespace hda
{
    namespace
    {
        Result ConfigureInputAmplifiers(IHDAFunctions& hdaFunctions, Node_AW& aw, int input_idx)
        {
            // Set input amplifiers - enable 'index_idx', mute everything else
            for (int index = 0; index < aw.aw_num_conn; index++) {
                using namespace verb::amp_gain_mute;
                uint32_t payload = Input | Left | Right | Index(index);
                if (index == input_idx)
                    payload |= Gain(30);
                else
                    payload |= Mute;
                uint32_t r;
                if (auto result = hdaFunctions.IssueVerb(
                        aw.MakeVerb(verb::MakePayload(verb::SetAmpGainMute, payload)), &r, nullptr);
                    result.IsFailure())
                    return result;
            }
            return Result::Success();
        }

        bool IsAWNodeUsedInRoutingPlan(RoutingPlan& rp, Node_AW& aw)
        {
            bool found = false;
            for (int i = 0; !found && i < rp.rp_num_nodes; i++)
                found = &aw == rp.rp_node[i];
            return found;
        }

        Result RouteNodeToAudioOut(IHDAFunctions& hdaFunctions, Node_AW& cur_aw, RoutingPlan& rp)
        {
            // Hook the input up to the list
            rp.rp_node[rp.rp_num_nodes++] = &cur_aw;
            if (cur_aw.GetType() == NT_AudioOut) {
                /*
                 * We've reached an 'audio out'; this is the end of the line as we
                 * can feed our audio output into.
                 */
                // Set output gain - XXX does the node support this?
                uint32_t r;
                using namespace verb::amp_gain_mute;
                if (auto result = hdaFunctions.IssueVerb(
                        cur_aw.MakeVerb(verb::MakePayload(
                            verb::SetAmpGainMute,
                            verb::amp_gain_mute::Output | Left | Right | Gain(30))),
                        &r, NULL);
                    result.IsFailure())
                    return result;

                return Result::Success();
            }

            if (cur_aw.GetType() == NT_AudioMixer) {
                // Audio mixers take >1 inputs and a single output
                for (int in_idx = 0; in_idx < cur_aw.aw_num_conn; in_idx++) {
                    auto& next_aw = *cur_aw.aw_conn[in_idx];
                    if (next_aw.GetType() != NT_AudioOut && next_aw.GetType() != NT_AudioSelector)
                        continue; // We can only connect audio output/selectors here
                    if (IsAWNodeUsedInRoutingPlan(rp, next_aw))
                        continue; // Already routed; we need another one

                    // Okay, this mixer connects to an unused node; connect this input to our mixer
                    // node
                    if (auto result = ConfigureInputAmplifiers(hdaFunctions, cur_aw, in_idx);
                        result.IsFailure())
                        return result;

                    // Set output gain
                    {
                        uint32_t r;
                        using namespace verb::amp_gain_mute;
                        if (auto result = hdaFunctions.IssueVerb(
                                cur_aw.MakeVerb(verb::MakePayload(
                                    verb::SetAmpGainMute,
                                    verb::amp_gain_mute::Output | Left | Right | Gain(60))),
                                &r, NULL);
                            result.IsFailure())
                            return result;
                    }

                    // Mixer was routed; we need to continue routing
                    return RouteNodeToAudioOut(hdaFunctions, next_aw, rp);
                }
                kprintf(
                    "TODO: ended up at nid 0x%x which we can't route to an audio-out/selector - "
                    "aborting",
                    cur_aw.n_address.na_nid);
                return RESULT_MAKE_FAILURE(ENOSPC); // XXX makes no sense?
            }

            if (cur_aw.GetType() == NT_AudioSelector) {
                // Audio selectors allow one input to be chosen out of multiple
                for (int in_idx = 0; in_idx < cur_aw.aw_num_conn; in_idx++) {
                    auto& next_aw = *cur_aw.aw_conn[in_idx];
                    if (next_aw.GetType() != NT_AudioOut && next_aw.GetType() != NT_AudioSelector)
                        continue; // We can only connect audio output/selectors here
                    if (IsAWNodeUsedInRoutingPlan(rp, next_aw))
                        continue; // Already routed; we need another one

                    // Set the input of the audio selector to whatever node providing input for us
                    uint32_t r;
                    if (auto result = hdaFunctions.IssueVerb(
                            cur_aw.MakeVerb(verb::MakePayload(verb::SetConnSelect, in_idx)), &r,
                            NULL);
                        result.IsFailure())
                        return result;

                    const auto payload = [](int in_idx) {
                        using namespace verb::amp_gain_mute;
                        return Input | Left | Right | Gain(60) | Index(in_idx);
                    }(in_idx);
                    if (auto result = hdaFunctions.IssueVerb(
                            cur_aw.MakeVerb(verb::MakePayload(verb::SetAmpGainMute, payload)), &r,
                            nullptr);
                        result.IsFailure())
                        return result;

                    // Set output gain - XXX does the node support this?
                    {
                        using namespace verb::amp_gain_mute;
                        if (auto result = hdaFunctions.IssueVerb(
                                cur_aw.MakeVerb(verb::MakePayload(
                                    verb::SetAmpGainMute,
                                    verb::amp_gain_mute::Output | Left | Right | Gain(60))),
                                &r, NULL);
                            result.IsFailure())
                            return result;
                    }

                    // Selector was routed; continue
                    return RouteNodeToAudioOut(hdaFunctions, next_aw, rp);
                }
                kprintf(
                    "TODO: ended up at nid 0x%x which we can't route to an audio-out/selector - "
                    "aborting",
                    cur_aw.n_address.na_nid);
                return RESULT_MAKE_FAILURE(ENOSPC); // XXX makes no sense?
            }

            kprintf(
                "TODO: ended up at unsupported node nid 0x%x type 0x%d - aborting",
                cur_aw.n_address.na_nid, cur_aw.GetType());
            return RESULT_MAKE_FAILURE(ENOSPC); // XXX makes no sense?
        }

    } // unnamed namespace

    /*
     * Given audio group 'afg' on device 'dev', this will route 'channels'-channel
     * output to output 'o'. On success, returns the kmalloc()'ed routing plan.
     */
    Result HDADevice::RouteOutput(AFG& afg, int channels, Output& o, RoutingPlan** rp)
    {
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
        for (auto& aw : afg.afg_nodes) {
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
            int ch_count =
                verb::param::aw_caps::ChanCountLSB(output_pin->aw_caps) + 1; /* XXX ext */
            channels_left -= ch_count;

            /*
             * Okay, we need to route to this output; make a list of everything that is
             * connected to here - we care only about mixers and outputs XXX we should
             * consider selectors too
             */
            HDA_VPRINTF(
                "will use %d-channel output %s-%s", ch_count,
                ResolveLocationToString(verb::cfg_default::Location(output_pin->p_cfg)),
                PinColorString[verb::cfg_default::Color(output_pin->p_cfg)]);
            if (output_pin->aw_num_conn == 0) {
                delete *rp;
                Printf(
                    "unable to route from cad %d nid %d; it has no outputs",
                    output_pin->n_address.na_cad, output_pin->n_address.na_nid);
                return RESULT_MAKE_FAILURE(ENOSPC); // XXX does this make sense?
            }

            /*
             * Walk through this pin's input list and start routing the very first item we see that
             * is acceptable.
             */
            Result result(RESULT_MAKE_FAILURE(ENOSPC)); // XXX does this make sense
            for (int pin_in_idx = 0; pin_in_idx < output_pin->aw_num_conn; pin_in_idx++) {
                /* pin_in_aw is the node connected to output_pin's input index pin_in_idx */
                Node_AW* pin_in_aw = output_pin->aw_conn[pin_in_idx];

                /* First see if we have already routed this item */
                if (IsAWNodeUsedInRoutingPlan(**rp, *pin_in_aw))
                    continue;

                /* Enable output on this pin */
                uint32_t r;
                {
                    using namespace verb::pin_control;
                    result = hdaFunctions->IssueVerb(
                        output_pin->MakeVerb(
                            verb::MakePayload(verb::SetPinControl, HEnable | OutEnable)),
                        &r, NULL);
                    if (result.IsFailure())
                        break;
                }

                /* Set input to what we found that should work */
                result = hdaFunctions->IssueVerb(
                    output_pin->MakeVerb(verb::MakePayload(verb::SetConnSelect, pin_in_idx)), &r,
                    NULL);
                if (result.IsFailure())
                    break;

                /* Set output gain XXX We should check if the pin supports this */
                {
                    using namespace verb::amp_gain_mute;
                    result = hdaFunctions->IssueVerb(
                        output_pin->MakeVerb(verb::MakePayload(
                            verb::SetAmpGainMute,
                            verb::amp_gain_mute::Output | Left | Right | Gain(60))),
                        &r, NULL);
                    if (result.IsFailure())
                        break;
                }

                /*
                 * Pin is routed to use input pin_in_aw; we must now configure it and
                 * whatever hooks to it.
                 */
                result = RouteNodeToAudioOut(*hdaFunctions, *pin_in_aw, **rp);
                if (result.IsFailure())
                    break;
            }

            if (result.IsFailure()) {
                kfree(*rp);
                return result;
            }
        }

        return Result::Success();
    }

} // namespace hda

/* vim:set ts=2 sw=2: */
