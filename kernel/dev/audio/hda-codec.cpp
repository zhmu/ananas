/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "hda.h"
#include "verb.h"

namespace hda
{
#if HDA_VERBOSE
    const char* const PinConnectivityString[] = {"jack", "none", "fixed", "both"};
    const char* const PinDefaultDeviceString[] = {"line-out",        "speaker",
                                                  "hp-out",          "cd",
                                                  "spdif-out",       "dig-other-out",
                                                  "modem-line-side", "modem-handset-side",
                                                  "line-in",         "aux",
                                                  "mic-in",          "telephony",
                                                  "spdif-in",        "digital-other-in",
                                                  "reserved",        "other"};
    const char* const PinConnectionTypeString[] = {
        "unknown",      "1/8\"", "1/4\"", "atapi", "rca",         "optical", "other-digital",
        "other-analog", "din",   "xlr",   "rj-11", "combination", "?c",      "?d",
        "?e",           "other"};
    const char* const PinColorString[] = {"unknown", "black",  "grey",   "blue", "green", "red",
                                          "orange",  "yellow", "purple", "pink", "?a",    "?b",
                                          "?c",      "?d",     "white",  "other"};
#endif

    Result HDADevice::FillAWConnectionList(Node_AW& aw)
    {
        if (!verb::param::aw_caps::ConnList(aw.aw_caps))
            return Result::Success();

        uint32_t r;
        if (auto result = hdaFunctions->IssueVerb(
                aw.MakeVerb(verb::MakePayload(verb::GetParam, verb::param::ConListLen)), &r, NULL);
            result.IsFailure())
            return result;
        int cl_len = verb::param::con_list_len::Length(r);
        if (verb::param::con_list_len::LongForm(r)) {
            Printf("unsupported long nentry form!");
            return Result::Failure(ENODEV);
        }

        aw.aw_num_conn = cl_len;
        aw.aw_conn = new Node_AW*[cl_len];

        for (int idx = 0; idx < cl_len; idx += 4) {
            if (auto result = hdaFunctions->IssueVerb(
                    aw.MakeVerb(verb::MakePayload(verb::GetConnListEntry, idx)), &r, NULL);
                result.IsFailure())
                return result;

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

        return Result::Success();
    }

    Result HDADevice::AttachWidget_AudioOut(Node_AudioOut& ao)
    {
        if (auto result = hdaFunctions->IssueVerb(
                ao.MakeVerb(verb::MakePayload(verb::GetParam, verb::param::PCM)), &ao.ao_pcm, NULL);
            result.IsFailure())
            return result;

        // Set power state to D0; XXX maybe we should only do this when actually playing things
        {
            uint32_t r;
            if (auto result = hdaFunctions->IssueVerb(
                    ao.MakeVerb(verb::MakePayload(verb::SetPowerState, 0)), &r, NULL);
                result.IsFailure())
                return result;
        }

        /* Use the AFG's default if the widget doesn't specify any */
        if (ao.ao_pcm == 0)
            ao.ao_pcm = hda_afg->afg_pcm;
        return Result::Success();
    }

    Result HDADevice::AttachWidget_AudioIn(Node_AudioIn& ai) { return Result::Success(); }

    Result HDADevice::AttachWidget_AudioMixer(Node_AudioMixer& am)
    {
        int aw_caps = am.aw_caps;

        am.am_ampcap_in = 0;
        am.am_ampcap_out = 0;

        if (verb::param::aw_caps::InAmpPresent(aw_caps)) {
            if (auto result = hdaFunctions->IssueVerb(
                    am.MakeVerb(verb::MakePayload(verb::GetParam, verb::param::AmpCap_In)),
                    &am.am_ampcap_in, NULL);
                result.IsFailure())
                return result;
        }

        if (verb::param::aw_caps::OutAmpPresent(aw_caps)) {
            if (auto result = hdaFunctions->IssueVerb(
                    am.MakeVerb(verb::MakePayload(verb::GetParam, verb::param::AmpCap_Out)),
                    &am.am_ampcap_out, NULL);
                result.IsFailure())
                return result;
        }

        return FillAWConnectionList(am);
    }

    Result HDADevice::AttachWidget_AudioSelector(Node_AudioSelector& as)
    {
        // XXX We should read the amplifier capabilities here
        return FillAWConnectionList(as);
    }

    Result HDADevice::AttachWidget_VolumeKnob(Node_VolumeKnob& vk)
    {
        uint32_t r;
        if (auto result = hdaFunctions->IssueVerb(
                vk.MakeVerb(verb::MakePayload(verb::GetVolumeKnobControl, 0)), &r, NULL);
            result.IsFailure())
            return result;

        Printf("volume knob level %d", r);

        // XXX We should read the amplifier capabilities here
        return FillAWConnectionList(vk);
    }

    Result HDADevice::AttachWidget_VendorDefined(Node_VendorDefined& vd)
    {
        return Result::Success();
    }

    Result HDADevice::AttachWidget_PinComplex(Node_Pin& p)
    {
        if (auto result = hdaFunctions->IssueVerb(
                p.MakeVerb(verb::MakePayload(verb::GetParam, verb::param::PinCap)), &p.p_cap, NULL);
            result.IsFailure())
            return result;

        if (auto result = hdaFunctions->IssueVerb(
                p.MakeVerb(verb::MakePayload(verb::GetCfgDefault, 0)), &p.p_cfg, NULL);
            result.IsFailure())
            return result;

        return FillAWConnectionList(p);
    }

    Result HDADevice::AttachNode_AFG(AFG& afg)
    {
        int cad = afg.afg_cad;
        int nid = afg.afg_nid;

        // Set power state to D0 XXX maybe we should only do this when actually playing things
        {
            uint32_t r;
            if (auto result = hdaFunctions->IssueVerb(
                    verb::Make(cad, nid, verb::MakePayload(verb::SetPowerState, 0)), &r, NULL);
                result.IsFailure())
                return result;
        }

        /* Fetch the default parameters; these are used if the subnodes don't supply them */
        uint32_t r;
        if (auto result = hdaFunctions->IssueVerb(
                verb::Make(cad, nid, verb::MakePayload(verb::GetParam, verb::param::PCM)),
                &afg.afg_pcm, NULL);
            result.IsFailure())
            return result;

        /* Again, audio subnodes have even more subnodes... query it here */
        if (auto result = hdaFunctions->IssueVerb(
                verb::Make(cad, nid, verb::MakePayload(verb::GetParam, verb::param::SubNodeCount)),
                &r, NULL);
            result.IsFailure())
            return result;
        const int sn_addr = verb::param::sub_node_count::Start(r);
        const int sn_total = verb::param::sub_node_count::Total(r);
        // HDA_DEBUG("hda_attach_node_afg(): codec %d nodeid=%x -> sn_addr %x sn_total %d", cad,
        // nodeid, sn_addr, sn_total);

        for (unsigned int sn = 0; sn < sn_total; sn++) {
            /* Obtain the audio widget capabilities of this node; this tells us what it is */
            int nid = sn_addr + sn;
            uint32_t aw_caps;
            if (auto result = hdaFunctions->IssueVerb(
                    verb::Make(cad, nid, verb::MakePayload(verb::GetParam, verb::param::AWCaps)),
                    &aw_caps, NULL);
                result.IsFailure())
                return result;

            Node_AW* aw = nullptr;
            NodeAddress nodeAddress(cad, nid);
            Result result = Result::Success();
            switch (verb::param::aw_caps::Type(aw_caps)) {
                case verb::param::aw_caps::type::AudioOut:
                    aw = new Node_AudioOut(nodeAddress, aw_caps);
                    result = AttachWidget_AudioOut(static_cast<Node_AudioOut&>(*aw));
                    break;
                case verb::param::aw_caps::type::AudioIn:
                    aw = new Node_AudioIn(nodeAddress, aw_caps);
                    result = AttachWidget_AudioIn(static_cast<Node_AudioIn&>(*aw));
                    break;
                case verb::param::aw_caps::type::AudioMixer:
                    aw = new Node_AudioMixer(nodeAddress, aw_caps);
                    result = AttachWidget_AudioMixer(static_cast<Node_AudioMixer&>(*aw));
                    break;
                case verb::param::aw_caps::type::AudioSelector:
                    aw = new Node_AudioSelector(nodeAddress, aw_caps);
                    result = AttachWidget_AudioSelector(static_cast<Node_AudioSelector&>(*aw));
                    break;
                case verb::param::aw_caps::type::AudioPinComplex:
                    aw = new Node_Pin(nodeAddress, aw_caps);
                    result = AttachWidget_PinComplex(static_cast<Node_Pin&>(*aw));
                    break;
                case verb::param::aw_caps::type::AudioVolumeKnob:
                    aw = new Node_VolumeKnob(nodeAddress, aw_caps);
                    result = AttachWidget_VolumeKnob(static_cast<Node_VolumeKnob&>(*aw));
                    break;
                case verb::param::aw_caps::type::AudioVendorDefined:
                    aw = new Node_VendorDefined(nodeAddress, aw_caps);
                    result = AttachWidget_VendorDefined(static_cast<Node_VendorDefined&>(*aw));
                    break;
                default:
                    Printf(
                        "ignored cad=%d nid=%d awtype=%d", cad, nid,
                        verb::param::aw_caps::Type(aw_caps));
                    break;
            }

            if (result.IsSuccess() && aw != nullptr)
                afg.afg_nodes.push_back(*aw);
        }

        /*
         * Okay, we are done - need to resolve the connection node ID's to pointers.
         * This is O(N^2)... XXX
         */
        for (auto& aw : afg.afg_nodes) {
            for (int n = 0; n < aw.aw_num_conn; n++) {
                uintptr_t nid = (uintptr_t)aw.aw_conn[n];
                Node_AW* aw_found = nullptr;
                for (auto& aw2 : afg.afg_nodes) {
                    if (aw2.n_address.na_nid != nid)
                        continue;
                    aw_found = &aw2;
                    break;
                }
                if (aw_found == nullptr) {
                    Printf(
                        "cad %d nid %d lists connection nid %d, but not found - aborting",
                        aw.n_address.na_cad, aw.n_address.na_nid, nid);
                    return Result::Failure(ENODEV);
                }
                aw.aw_conn[n] = aw_found;
            }
        }

        return Result::Success();
    }

#if HDA_VERBOSE
    const char* ResolveLocationToString(int location)
    {
        switch ((location >> 4) & 3) {
            case 0: /* external on primary chassis */
                switch (location & 0xf) {
                    case 0:
                        return "ext/n/a";
                    case 1:
                        return "ext/rear";
                    case 2:
                        return "ext/front";
                    case 3:
                        return "ext/left";
                    case 4:
                        return "ext/right";
                    case 5:
                        return "ext/top";
                    case 6:
                        return "ext/bottom";
                    case 7:
                        return "ext/rear";
                    case 8:
                        return "ext/drive-bay";
                }
                break;
            case 1: /* internal */
                switch (location & 0xf) {
                    case 0:
                        return "int/n/a";
                    case 7:
                        return "int/riser";
                    case 8:
                        return "int/display";
                    case 9:
                        return "int/atapi";
                }
                break;
            case 2: /* separate chassis */
                switch (location & 0xf) {
                    case 0:
                        return "sep/n/a";
                    case 1:
                        return "sep/rear";
                    case 2:
                        return "sep/front";
                    case 3:
                        return "sep/left";
                    case 4:
                        return "sep/right";
                    case 5:
                        return "sep/top";
                    case 6:
                        return "sep/bottom";
                }
                break;
            case 3: /* other */
                switch (location & 0xf) {
                    case 0:
                        return "oth/n/a";
                    case 6:
                        return "oth/bottom";
                    case 7:
                        return "oth/lid-inside";
                    case 8:
                        return "oth/lid-outside";
                }
                break;
        }
        return "?";
    }

    void DumpAFG(AFG& afg)
    {
        if (afg.afg_nodes.empty())
            return;

        static const char* aw_node_types[] = {"pc", "ao", "ai", "am", "as", "vd"};
        for (auto& aw : afg.afg_nodes) {
            kprintf(
                "cad %x nid % 2x type %s ->", aw.n_address.na_cad, aw.n_address.na_nid,
                aw_node_types[aw.GetType()]);

            switch (aw.GetType()) {
                case NT_Pin: {
                    auto& p = static_cast<Node_Pin&>(aw);
                    kprintf(" [");
                    if (verb::param::pin_cap::HBR(p.p_cap))
                        kprintf(" hbr");
                    if (verb::param::pin_cap::DP(p.p_cap))
                        kprintf(" dp");
                    if (verb::param::pin_cap::EAPD(p.p_cap))
                        kprintf(" eapd");
                    if (verb::param::pin_cap::VRefControl(p.p_cap))
                        kprintf(" vref-control");
                    if (verb::param::pin_cap::HDMI(p.p_cap))
                        kprintf(" hdmi");
                    if (verb::param::pin_cap::BalancedIO(p.p_cap))
                        kprintf(" balanced-io");
                    if (verb::param::pin_cap::Input(p.p_cap))
                        kprintf(" input");
                    if (verb::param::pin_cap::Output(p.p_cap))
                        kprintf(" output");
                    if (verb::param::pin_cap::Headphone(p.p_cap))
                        kprintf(" headphone");
                    if (verb::param::pin_cap::Presence(p.p_cap))
                        kprintf(" presence");
                    if (verb::param::pin_cap::Trigger(p.p_cap))
                        kprintf(" trigger");
                    if (verb::param::pin_cap::Impedance(p.p_cap))
                        kprintf(" impedance");

                    {
                        using namespace verb::cfg_default;
                        kprintf(
                            " ] %s %s %s (%s) %s default-ass/seq %d/%d",
                            ResolveLocationToString(Location(p.p_cfg)),
                            PinConnectionTypeString[ConnectionType(p.p_cfg)],
                            PinConnectivityString[PortConnectivity(p.p_cfg)],
                            PinColorString[Color(p.p_cfg)],
                            PinDefaultDeviceString[DefaultDevice(p.p_cfg)],
                            DefaultAssociation(p.p_cfg), Sequence(p.p_cfg));
                    }
                    break;
                }
                case NT_AudioOut: {
                    auto& ao = static_cast<Node_AudioOut&>(aw);

                    using namespace verb::param::pcm;
                    kprintf(" formats [");
                    if (B32(ao.ao_pcm))
                        kprintf(" 32-bit");
                    if (B24(ao.ao_pcm))
                        kprintf(" 24-bit");
                    if (B20(ao.ao_pcm))
                        kprintf(" 20-bit");
                    if (B16(ao.ao_pcm))
                        kprintf(" 16-bit");
                    if (B8(ao.ao_pcm))
                        kprintf(" 8-bit");
                    kprintf(" ] rate [");
                    if (R8_0(ao.ao_pcm))
                        kprintf(" 8.0kHz");
                    if (R11_025(ao.ao_pcm))
                        kprintf(" 11.025kHz");
                    if (R16_0(ao.ao_pcm))
                        kprintf(" 16.0kHz");
                    if (R22_05(ao.ao_pcm))
                        kprintf(" 22.05kHz");
                    if (R32_0(ao.ao_pcm))
                        kprintf(" 32.0kHz");
                    if (R44_1(ao.ao_pcm))
                        kprintf(" 44.1kHz");
                    if (R48_0(ao.ao_pcm))
                        kprintf(" 48.0kHz");
                    if (R88_2(ao.ao_pcm))
                        kprintf(" 88.2kHz");
                    if (R96_0(ao.ao_pcm))
                        kprintf(" 96.0kHz");
                    if (R176_4(ao.ao_pcm))
                        kprintf(" 172.4kHz");
                    if (R192_0(ao.ao_pcm))
                        kprintf(" 192.0kHz");
                    if (R384_0(ao.ao_pcm))
                        kprintf(" 384.0kHz");
                    kprintf(" ]");
                }
                default:
                    break;
            }

            if (aw.aw_num_conn > 0) {
                kprintf(" conn [");
                for (int n = 0; n < aw.aw_num_conn; n++) {
                    kprintf("%s%x", (n > 0) ? "," : "", aw.aw_conn[n]->n_address.na_nid);
                }
                kprintf("]");
            }
            kprintf("\n");
        }

        for (auto& o : afg.afg_outputs) {
            kprintf("output: %d-channel ->", o.o_channels);
            for (int n = 0; n < o.o_pingroup->pg_count; n++) {
                Node_Pin* p = o.o_pingroup->pg_pin[n];
                kprintf(
                    " %s-%s", ResolveLocationToString(verb::cfg_default::Location(p->p_cfg)),
                    PinColorString[verb::cfg_default::Color(p->p_cfg)]);
            }
            kprintf("\n");
        }
    }
#endif /* HDA_VERBOSE */

    Result HDADevice::AttachPin_Render(AFG& afg, int association, int num_pins)
    {
        auto pg = new PinGroup;
        pg->pg_pin = new Node_Pin*[num_pins];
        pg->pg_count = 0;

        /* Now locate each pin and put it into place */
        int num_channels = 0;
        for (unsigned int seq = 0; seq < 16; seq++) {
            Node_Pin* found_pin = nullptr;
            for (auto& aw : afg.afg_nodes) {
                if (aw.GetType() != NT_Pin)
                    continue;
                auto& p = static_cast<Node_Pin&>(aw);
                if (verb::cfg_default::DefaultAssociation(p.p_cfg) != association)
                    continue;
                if (verb::cfg_default::Sequence(p.p_cfg) != seq)
                    continue;
                found_pin = &p;

                int ch_count = verb::param::aw_caps::ChanCountLSB(p.aw_caps) + 1; /* XXX ext */
                num_channels += ch_count;
                break;
            }

            if (found_pin != nullptr)
                pg->pg_pin[pg->pg_count++] = found_pin;
        }

        /* Make sure we have found all pins (this may be paranoid) */
        if (pg->pg_count != num_pins) {
            delete pg;
            return Result::Failure(ENODEV);
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
        if (auto result = RouteOutput(afg, num_channels, *o, &rp); result.IsFailure()) {
            // Cannot establish a routing plan to here -> ignore it
            delete pg;
            delete o;
            kprintf("unable to route %d channel output, ignoring\n", num_channels);
            return Result::Success();
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

        afg.afg_outputs.push_back(*o);
        return Result::Success();
    }

    Result HDADevice::AttachAFG(AFG& afg)
    {
        if (afg.afg_nodes.empty())
            return Result::Failure(ENODEV);

        /* Walk through all associations and see what we have got there */
        for (unsigned int association = 1; association < 15; association++) {
            int num_input = 0, num_output = 0;
            for (auto& aw : afg.afg_nodes) {
                if (aw.GetType() != NT_Pin)
                    continue;
                auto& p = static_cast<Node_Pin&>(aw);
                if (verb::cfg_default::DefaultAssociation(p.p_cfg) != association)
                    continue;

                if (verb::param::pin_cap::Input(p.p_cap))
                    num_input++;
                if (verb::param::pin_cap::Output(p.p_cap))
                    num_output++;
            }
            if (num_input == 0 && num_output == 0)
                continue;

            HDA_VPRINTF(
                "association %d has %d input(s), %d output(s)", association, num_input, num_output);

            // XXX We only support outputs here
            if (num_output > 0)
                if (auto result = AttachPin_Render(afg, association, num_output);
                    result.IsFailure())
                    return result;
        }

        return Result::Success();
    }

    Result HDADevice::AttachNode(int cad, int nodeid)
    {
        /*
         * First step is to figure out the start address and subnode count; we'll
         * need to dive into the subnodes.
         */
        uint32_t r;
        if (auto result = hdaFunctions->IssueVerb(
                verb::Make(
                    cad, nodeid, verb::MakePayload(verb::GetParam, verb::param::SubNodeCount)),
                &r, NULL);
            result.IsFailure())
            return result;
        const int sn_addr = verb::param::sub_node_count::Start(r);
        const int sn_total = verb::param::sub_node_count::Total(r);

        /* Now dive deeper and see what the subnodes can do */
        hda_afg = NULL;
        for (unsigned int sn = 0; sn < sn_total; sn++) {
            int nid = sn_addr + sn;
            if (auto result = hdaFunctions->IssueVerb(
                    verb::Make(cad, nid, verb::MakePayload(verb::GetParam, verb::param::FGT)), &r,
                    NULL);
                result.IsFailure())
                return result;
            int fgt_type = verb::param::fgt::NodeType(r);
            if (fgt_type != verb::param::fgt::node_type::Audio)
                continue; /* we don't care about non-audio types */

            if (hda_afg != NULL) {
                Printf("warning: ignoring afg %d (we only support one per device)", nid);
                continue;
            }

            /* Now dive into this subnode and attach it */
            auto afg = new AFG;
            afg->afg_cad = cad;
            afg->afg_nid = nid;
            hda_afg = afg;

            if (auto result = AttachNode_AFG(*afg); result.IsFailure())
                return result;

            if (auto result = AttachAFG(*afg); result.IsFailure())
                return result;
            hda_afg = afg;

#if HDA_VERBOSE
            DumpAFG(*afg);
#endif
        }

        return Result::Success();
    }

} // namespace hda
