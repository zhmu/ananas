#include <ananas/types.h>
#include <ananas/dev/hda.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/trace.h>
#include "hda.h"

TRACE_SETUP;

#if HDA_VERBOSE
const char* const hda_pin_connectivity_string[] = { "jack", "none", "fixed", "both" };
const char* const hda_pin_default_device_string[] = {
 "line-out", "speaker", "hp-out", "cd", "spdif-out", "dig-other-out",
 "modem-line-side", "modem-handset-side", "line-in", "aux", "mic-in",
 "telephony", "spdif-in", "digital-other-in", "reserved", "other" };
const char* const hda_pin_connection_type_string[] = {
 "unknown", "1/8\"", "1/4\"", "atapi", "rca", "optical", "other-digital",
 "other-analog", "din", "xlr", "rj-11", "combination", "?c", "?d", "?e", "other" };
const char* const hda_pin_color_string[] = {
 "unknown", "black", "grey", "blue", "green", "red", "orange", "yellow",
 "purple", "pink", "?a", "?b", "?c", "?d", "white", "other"
};
#endif

static errorcode_t
hda_fill_aw_connlist(device_t dev, struct HDA_NODE_AW* aw)
{
	struct HDA_PRIVDATA* privdata = dev->privdata;
	struct HDA_DEV_FUNC* devfuncs = privdata->hda_dev_func;
	if (!HDA_PARAM_AW_CAPS_CONNLIST(aw->aw_caps))
		return ANANAS_ERROR_NONE;

	uint32_t r;
	errorcode_t err = devfuncs->hdf_issue_verb(dev, HDA_MAKE_VERB_NODE(aw, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_CONLISTLEN)), &r, NULL);
	ANANAS_ERROR_RETURN(err);
	int cl_len = HDA_CODEC_PARAM_CONLISTLEN_LENGTH(r);
	if (HDA_CODEC_PARAM_CONLISTLEN_LONGFORM(r)) {
		device_printf(dev, "unsupported long nentry form!");
		return ANANAS_ERROR(NO_DEVICE);
	}


	aw->aw_num_conn = cl_len;
	aw->aw_conn = kmalloc(sizeof(struct HDA_NODE_AW*) * cl_len);

	for (int idx = 0; idx < cl_len; idx += 4) {
		err = devfuncs->hdf_issue_verb(dev, HDA_MAKE_VERB_NODE(aw, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETCONNLISTENTRY, idx)), &r, NULL);
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
			aw->aw_conn[idx + n] = (struct HDA_NODE_AW*)(addr_t)e;
		}
	}

	return ANANAS_ERROR_NONE;
}

static errorcode_t
hda_attach_widget_audioout(device_t dev, struct HDA_NODE_AUDIOOUT* ao)
{
	struct HDA_PRIVDATA* privdata = dev->privdata;
	struct HDA_DEV_FUNC* devfuncs = privdata->hda_dev_func;

	errorcode_t err = devfuncs->hdf_issue_verb(dev, HDA_MAKE_VERB_NODE(ao, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_PCM)), &ao->ao_pcm, NULL);
	ANANAS_ERROR_RETURN(err);

	/* Use the AFG's default if the widget doesn't specify any */
	if (ao->ao_pcm == 0)
		ao->ao_pcm = privdata->hda_afg->afg_pcm;
	return ANANAS_ERROR_NONE;
}

static errorcode_t
hda_attach_widget_audioin(device_t dev, struct HDA_NODE_AUDIOIN* ai)
{
	return ANANAS_ERROR_NONE;
}

static errorcode_t
hda_attach_widget_audiomixer(device_t dev, struct HDA_NODE_AUDIOMIXER* am)
{
	struct HDA_PRIVDATA* privdata = dev->privdata;
	struct HDA_DEV_FUNC* devfuncs = privdata->hda_dev_func;
	int aw_caps = am->am_node.aw_caps;

	errorcode_t err;
	am->am_ampcap_in = 0;
	am->am_ampcap_out = 0;
	
	if (HDA_PARAM_AW_CAPS_INAMPPRESENT(aw_caps)) {
		err = devfuncs->hdf_issue_verb(dev, HDA_MAKE_VERB_NODE(am, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_AMPCAP_IN)), &am->am_ampcap_in, NULL);
		ANANAS_ERROR_RETURN(err);
	}

	if (HDA_PARAM_AW_CAPS_OUTAMPPRESENT(aw_caps)) {
		err = devfuncs->hdf_issue_verb(dev, HDA_MAKE_VERB_NODE(am, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_AMPCAP_OUT)), &am->am_ampcap_out, NULL);
		ANANAS_ERROR_RETURN(err);
	}

	err = hda_fill_aw_connlist(dev, &am->am_node);
	ANANAS_ERROR_RETURN(err);
	return ANANAS_ERROR_NONE;
}

static errorcode_t
hda_attach_widget_audioselector(device_t dev, struct HDA_NODE_AUDIOSELECTOR* as)
{
	errorcode_t err;
	err = hda_fill_aw_connlist(dev, &as->as_node);
	ANANAS_ERROR_RETURN(err);

	kprintf("hda_attach_widget_audioselector: TODO\n");

	return ANANAS_ERROR_NONE;
}

static errorcode_t
hda_attach_widget_vendordefined(device_t dev, struct HDA_NODE_VENDORDEFINED* vd)
{
	return ANANAS_ERROR_NONE;
}

static errorcode_t
hda_attach_widget_pincomplex(device_t dev, struct HDA_NODE_PIN* p)
{
	struct HDA_PRIVDATA* privdata = dev->privdata;
	struct HDA_DEV_FUNC* devfuncs = privdata->hda_dev_func;

	errorcode_t err = devfuncs->hdf_issue_verb(dev, HDA_MAKE_VERB_NODE(p, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_PINCAP)), &p->p_cap, NULL);
	ANANAS_ERROR_RETURN(err);
	
	err = devfuncs->hdf_issue_verb(dev, HDA_MAKE_VERB_NODE(p, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETCFGDEFAULT, 0)), &p->p_cfg, NULL);
	ANANAS_ERROR_RETURN(err);

	err = hda_fill_aw_connlist(dev, &p->p_node);
	ANANAS_ERROR_RETURN(err);
	return ANANAS_ERROR_NONE;
}

static errorcode_t
hda_attach_node_afg(device_t dev, struct HDA_AFG* afg)
{
	struct HDA_PRIVDATA* privdata = dev->privdata;
	struct HDA_DEV_FUNC* devfuncs = privdata->hda_dev_func;
	int cad = afg->afg_cad;
	int nid = afg->afg_nid;

	/* Fetch the default parameters; these are used if the subnodes don't supply them */
	uint32_t r;
	errorcode_t err = devfuncs->hdf_issue_verb(dev, HDA_MAKE_VERB(cad, nid, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_PCM)), &afg->afg_pcm, NULL);
	ANANAS_ERROR_RETURN(err);

	/* Again, audio subnodes have even more subnodes... query it here */
	err = devfuncs->hdf_issue_verb(dev, HDA_MAKE_VERB(cad, nid, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_SUBNODECOUNT)), &r, NULL);
	ANANAS_ERROR_RETURN(err);
	int sn_addr = HDA_PARAM_SUBNODECOUNT_START(r);
	int sn_total = HDA_PARAM_SUBNODECOUNT_TOTAL(r);
	//HDA_DEBUG("hda_attach_node_afg(): codec %d nodeid=%x -> sn_addr %x sn_total %d", cad, nodeid, sn_addr, sn_total);

	for (unsigned int sn = 0; sn < sn_total; sn++) {
		/* Obtain the audio widget capabilities of this node; this tells us what it is */
		int nid = sn_addr + sn;
		uint32_t aw_caps;
		err = devfuncs->hdf_issue_verb(dev, HDA_MAKE_VERB(cad, nid, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_AW_CAPS)), &aw_caps, NULL);
		ANANAS_ERROR_RETURN(err);

#define HDA_AWNODE_INIT(p, type) \
	aw = (struct HDA_NODE_AW*)p; \
	aw->aw_node.n_type = (type); \
	aw->aw_node.n_cad = cad; \
	aw->aw_node.n_nid = nid; \
	aw->aw_caps = aw_caps; \
	aw->aw_num_conn = 0; \
	aw->aw_conn = NULL

		struct HDA_NODE_AW* aw = NULL;
		switch(HDA_PARAM_AW_CAPS_TYPE(aw_caps)) {
			case HDA_PARAM_AW_CAPS_TYPE_AUDIO_OUT: {
				struct HDA_NODE_AUDIOOUT* ao = kmalloc(sizeof *ao);
				HDA_AWNODE_INIT(ao, NT_AudioOut);
				err = hda_attach_widget_audioout(dev, ao);
				break;
			}
			case HDA_PARAM_AW_CAPS_TYPE_AUDIO_IN: {
				struct HDA_NODE_AUDIOIN* ai = kmalloc(sizeof *ai);
				HDA_AWNODE_INIT(ai, NT_AudioIn);
				err = hda_attach_widget_audioin(dev, ai);
				break;
			}
			case HDA_PARAM_AW_CAPS_TYPE_AUDIO_MIXER: {
				struct HDA_NODE_AUDIOMIXER* am = kmalloc(sizeof *am);
				HDA_AWNODE_INIT(am, NT_AudioMixer);
				err = hda_attach_widget_audiomixer(dev, am);
				break;
			}
			case HDA_PARAM_AW_CAPS_TYPE_AUDIO_SELECTOR: {
				struct HDA_NODE_AUDIOSELECTOR* as = kmalloc(sizeof *as);
				HDA_AWNODE_INIT(as, NT_AudioSelector);
				err = hda_attach_widget_audioselector(dev, as);
				break;
			}
			case HDA_PARAM_AW_CAPS_TYPE_AUDIO_PINCOMPLEX: {
				struct HDA_NODE_PIN* p = kmalloc(sizeof *p);
				HDA_AWNODE_INIT(p, NT_Pin);
				err = hda_attach_widget_pincomplex(dev, p);
				break;
			}
			case HDA_PARAM_AW_CAPS_TYPE_AUDIO_VENDORDEFINED: {
				struct HDA_NODE_VENDORDEFINED* vd = kmalloc(sizeof *vd);
				HDA_AWNODE_INIT(vd, NT_VendorDefined);
				err = hda_attach_widget_vendordefined(dev, vd);
				break;
			}
			default:
				device_printf(dev, "ignored cad=%d nid=%d awtype=%d", cad, nid, HDA_PARAM_AW_CAPS_TYPE(aw_caps));
				break;
		}

		if (aw != NULL)
			LIST_APPEND(&afg->afg_nodes, aw);

#undef HDA_AWNODE_INIT
	}

	/*
	 * Okay, we are done - need to resolve the connection node ID's to pointers.
	 * This is O(N^2)... XXX
	 */
	LIST_FOREACH(&afg->afg_nodes, aw, struct HDA_NODE_AW) {
		for(int n = 0; n < aw->aw_num_conn; n++) {
			uintptr_t nid = (uintptr_t)aw->aw_conn[n];
			struct HDA_NODE_AW* aw_found = NULL;
			LIST_FOREACH(&afg->afg_nodes, aw2, struct HDA_NODE_AW) {
				if (aw2->aw_node.n_nid != nid)
					continue;
				aw_found = aw2;
				break;
			}
			if (aw_found == NULL) {
				device_printf(dev, "cad %d nid %d lists connection nid %d, but not found - aborting",
				 aw->aw_node.n_cad, aw->aw_node.n_nid, nid);
				return ANANAS_ERROR(NO_DEVICE);
			}
			aw->aw_conn[n] = aw_found;
		}
	}

	return err;
}

#if HDA_VERBOSE
const char*
hda_resolve_location(int location)
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

static void
hda_dump_afg(struct HDA_AFG* afg)
{
	if (LIST_EMPTY(&afg->afg_nodes))
		return;

	static const char* aw_node_types[] = {
		"pc", "ao", "ai", "am", "as", "vd"
	};
	LIST_FOREACH(&afg->afg_nodes, aw, struct HDA_NODE_AW) {
		kprintf("cad %x nid % 2x type %s ->", aw->aw_node.n_cad, aw->aw_node.n_nid, aw_node_types[aw->aw_node.n_type]);

		switch(aw->aw_node.n_type) {
			case NT_Pin: {
				struct HDA_NODE_PIN* p = (struct HDA_NODE_PIN*)aw;
				kprintf(" [");
				if (HDA_CODEC_PINCAP_HBR(p->p_cap))
					kprintf(" hbr");
				if (HDA_CODEC_PINCAP_DP(p->p_cap))
					kprintf(" dp");
				if (HDA_CODEC_PINCAP_EAPD(p->p_cap))
					kprintf(" eapd");
				if (HDA_CODEC_PINCAP_VREF_CONTROL(p->p_cap))
					kprintf(" vref-control");
				if (HDA_CODEC_PINCAP_HDMI(p->p_cap))
					kprintf(" hdmi");
				if (HDA_CODEC_PINCAP_BALANCED_IO(p->p_cap))
					kprintf(" balanced-io");
				if (HDA_CODEC_PINCAP_INPUT(p->p_cap))
					kprintf(" input");
				if (HDA_CODEC_PINCAP_OUTPUT(p->p_cap))
					kprintf(" output");
				if (HDA_CODEC_PINCAP_HEADPHONE(p->p_cap))
					kprintf(" headphone");
				if (HDA_CODEC_PINCAP_PRESENCE(p->p_cap))
					kprintf(" presence");
				if (HDA_CODEC_PINCAP_TRIGGER(p->p_cap))
					kprintf(" trigger");
				if (HDA_CODEC_PINCAP_IMPEDANCE(p->p_cap))
					kprintf(" impedance");

				kprintf(" ] %s %s %s (%s) %s default-ass/seq %d/%d",
				 hda_resolve_location(HDA_CODEC_CFGDEFAULT_LOCATION(p->p_cfg)),
				 hda_pin_connection_type_string[HDA_CODEC_CFGDEFAULT_CONNECTION_TYPE(p->p_cfg)],
				 hda_pin_connectivity_string[HDA_CODEC_CFGDEFAULT_PORTCONNECTIVITY(p->p_cfg)],
				 hda_pin_color_string[HDA_CODEC_CFGDEFAULT_COLOR(p->p_cfg)],
				 hda_pin_default_device_string[HDA_CODEC_CFGDEFAULT_DEFAULTDEVICE(p->p_cfg)],
				 HDA_CODEC_CFGDEFAULT_DEFAULT_ASSOCIATION(p->p_cfg),
				 HDA_CODEC_CFGDEFAULT_SEQUENCE(p->p_cfg));
				break;
			}
			case NT_AudioOut: {
				struct HDA_NODE_AUDIOOUT* ao = (struct HDA_NODE_AUDIOOUT*)aw;

				kprintf(" formats [");
				if (HDA_CODEC_PARAM_PCM_B32(ao->ao_pcm))
					kprintf(" 32-bit");
				if (HDA_CODEC_PARAM_PCM_B24(ao->ao_pcm))
					kprintf(" 24-bit");
				if (HDA_CODEC_PARAM_PCM_B20(ao->ao_pcm))
					kprintf(" 20-bit");
				if (HDA_CODEC_PARAM_PCM_B16(ao->ao_pcm))
					kprintf(" 16-bit");
				if (HDA_CODEC_PARAM_PCM_B8(ao->ao_pcm))
					kprintf(" 8-bit");
				kprintf(" ] rate [");
				if (HDA_CODEC_PARAM_PCM_R_8_0(ao->ao_pcm))
					kprintf(" 8.0kHz");
				if (HDA_CODEC_PARAM_PCM_R_11_025(ao->ao_pcm))
					kprintf(" 11.025kHz");
				if (HDA_CODEC_PARAM_PCM_R_16_0(ao->ao_pcm))
					kprintf(" 16.0kHz");
				if (HDA_CODEC_PARAM_PCM_R_22_05(ao->ao_pcm))
					kprintf(" 22.05kHz");
				if (HDA_CODEC_PARAM_PCM_R_32_0(ao->ao_pcm))
					kprintf(" 32.0kHz");
				if (HDA_CODEC_PARAM_PCM_R_44_1(ao->ao_pcm))
					kprintf(" 44.1kHz");
				if (HDA_CODEC_PARAM_PCM_R_48_0(ao->ao_pcm))
					kprintf(" 48.0kHz");
				if (HDA_CODEC_PARAM_PCM_R_88_2(ao->ao_pcm))
					kprintf(" 88.2kHz");
				if (HDA_CODEC_PARAM_PCM_R_96_0(ao->ao_pcm))
					kprintf(" 96.0kHz");
				if (HDA_CODEC_PARAM_PCM_R_176_4(ao->ao_pcm))
					kprintf(" 172.4kHz");
				if (HDA_CODEC_PARAM_PCM_R_192_0(ao->ao_pcm))
					kprintf(" 192.0kHz");
				if (HDA_CODEC_PARAM_PCM_R_384_0(ao->ao_pcm))
					kprintf(" 384.0kHz");
				kprintf(" ]");

			}
			default:
				break;
		}

		if (aw->aw_num_conn > 0) {
			kprintf(" conn [");
			for(int n = 0; n < aw->aw_num_conn; n++) {
				kprintf("%s%x", (n > 0) ? "," : "", aw->aw_conn[n]->aw_node.n_nid);
			}
			kprintf("]");
		}
		kprintf("\n");
	}

	LIST_FOREACH(&afg->afg_outputs, o, struct HDA_OUTPUT) {
		kprintf("output: %d-channel ->", o->o_channels);
		for (int n = 0; n < o->o_pingroup->pg_count; n++) {
			struct HDA_NODE_PIN* p = o->o_pingroup->pg_pin[n];
			kprintf(" %s-%s",
			 hda_resolve_location(HDA_CODEC_CFGDEFAULT_LOCATION(p->p_cfg)),
			 hda_pin_color_string[HDA_CODEC_CFGDEFAULT_COLOR(p->p_cfg)]);
		}
		kprintf("\n");
	}
}
#endif /* HDA_VERBOSE */

static errorcode_t
hda_attach_multipin_render(device_t dev, struct HDA_AFG* afg, int association, int num_pins)
{
	struct HDA_PINGROUP* pg = kmalloc(sizeof *pg + sizeof(struct HDA_NODE_PIN*) * num_pins);
	pg->pg_count = 0;

	/* Now locate each pin and put it into place */
	int num_channels = 0;
	for (unsigned int seq = 0; seq < 16; seq++) {
		struct HDA_NODE_PIN* found_pin = NULL;
		LIST_FOREACH(&afg->afg_nodes, aw, struct HDA_NODE_AW) {
			if (aw->aw_node.n_type != NT_Pin)
				continue;
			struct HDA_NODE_PIN* p = (struct HDA_NODE_PIN*)aw;
			if (HDA_CODEC_CFGDEFAULT_DEFAULT_ASSOCIATION(p->p_cfg) != association)
				continue;
			if (HDA_CODEC_CFGDEFAULT_SEQUENCE(p->p_cfg) != seq)
				continue;
			found_pin = p;

			int ch_count = HDA_PARAM_AW_CAPS_CHANCOUNTLSB(p->p_node.aw_caps) + 1; /* XXX ext */
			num_channels += ch_count;
			break;
		}

		if (found_pin != NULL)
			pg->pg_pin[pg->pg_count++] = found_pin;
	}

	/* Make sure we have found all pins (this may be paranoid) */
	if (pg->pg_count != num_pins)
			return ANANAS_ERROR(NO_DEVICE);

	/* Construct the output provider */
	struct HDA_OUTPUT* o = kmalloc(sizeof *o);
	o->o_channels = num_channels;
	o->o_pingroup = pg;
	o->o_pcm_output = 0;

	/*
	 * Let's see if we can actually route towards this entry; we use this to determine
	 * the maximum sample rate and such that we can use.
	 */
	struct HDA_ROUTING_PLAN* rp;
	errorcode_t err = hda_route_output(dev, afg, num_channels, o, &rp);
	if (err != ANANAS_ERROR_OK) {
		kfree(pg);
		kfree(o);
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
		struct HDA_NODE_AUDIOOUT* ao = (struct HDA_NODE_AUDIOOUT*)rp->rp_node[n];
		if (ao->ao_node.aw_node.n_type != NT_AudioOut)
			continue;
		if (o->o_pcm_output == 0)
			o->o_pcm_output = ao->ao_pcm; /* nothing yet; use the first as base */
		else
			o->o_pcm_output &= ao->ao_pcm; /* combine what the next output has */
	}
	kfree(rp);

	LIST_APPEND(&afg->afg_outputs, o);
	return ANANAS_ERROR_NONE;
}

static errorcode_t
hda_attach_singlepin_render(device_t dev, struct HDA_AFG* afg, int association)
{
	return hda_attach_multipin_render(dev, afg, association, 1);
}

static errorcode_t
hda_attach_afg(device_t dev, struct HDA_AFG* afg)
{
	if (LIST_EMPTY(&afg->afg_nodes))
		return ANANAS_ERROR(NO_DEVICE);

	/* Walk through all associations and see what we have got there */
	for (unsigned int association = 1; association < 15; association++) {
		int total_pins = 0;
		int num_input = 0, num_output = 0;
		LIST_FOREACH(&afg->afg_nodes, aw, struct HDA_NODE_AW) {
			if (aw->aw_node.n_type != NT_Pin)
				continue;
			struct HDA_NODE_PIN* p = (struct HDA_NODE_PIN*)aw;
			if (HDA_CODEC_CFGDEFAULT_DEFAULT_ASSOCIATION(p->p_cfg) != association)
				continue;

			total_pins++;
			if (HDA_CODEC_PINCAP_INPUT(p->p_cap))
				num_input++;
			if (HDA_CODEC_PINCAP_OUTPUT(p->p_cap))
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
				errorcode_t err = hda_attach_multipin_render(dev, afg, association, total_pins);
				ANANAS_ERROR_RETURN(err);
			} else /* total_pins == 1 */ {
				/* Single-pin rendering device */
				errorcode_t err = hda_attach_singlepin_render(dev, afg, association);
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

	return ANANAS_ERROR_NONE;
}


errorcode_t
hda_attach_node(device_t dev, int cad, int nodeid)
{
	struct HDA_PRIVDATA* privdata = dev->privdata;
	struct HDA_DEV_FUNC* devfuncs = privdata->hda_dev_func;

	/*
	 * First step is to figure out the start address and subnode count; we'll
	 * need to dive into the subnodes.
	 */
	uint32_t r;
	errorcode_t err = devfuncs->hdf_issue_verb(dev, HDA_MAKE_VERB(cad, nodeid, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_SUBNODECOUNT)), &r, NULL);
	ANANAS_ERROR_RETURN(err);
	int sn_addr = HDA_PARAM_SUBNODECOUNT_START(r);
	int sn_total = HDA_PARAM_SUBNODECOUNT_TOTAL(r);

	/* Now dive deeper and see what the subnodes can do */
	privdata->hda_afg = NULL;
	for (unsigned int sn = 0; sn < sn_total; sn++) {
		int nid = sn_addr + sn;
		err = devfuncs->hdf_issue_verb(dev, HDA_MAKE_VERB(cad, nid, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_GETPARAM, HDA_CODEC_PARAM_FGT)), &r, NULL);
		ANANAS_ERROR_RETURN(err);
		int fgt_type = HDA_PARAM_FGT_NODETYPE(r);
		if (fgt_type != HDA_PARAM_FGT_NODETYPE_AUDIO)
			continue; /* we don't care about non-audio types */
	
		if (privdata->hda_afg != NULL) {
			device_printf(dev, "warning: ignoring afg %d (we only support one per device)", nid);
			continue;
		}

		/* Now dive into this subnode and attach it */
		struct HDA_AFG* afg = kmalloc(sizeof* afg);
		afg->afg_cad = cad;
		afg->afg_nid = nid;
		LIST_INIT(&afg->afg_nodes);
		LIST_INIT(&afg->afg_outputs);
		privdata->hda_afg = afg;

		err = hda_attach_node_afg(dev, afg);
		ANANAS_ERROR_RETURN(err);

		err = hda_attach_afg(dev, afg);
		ANANAS_ERROR_RETURN(err);
		privdata->hda_afg = afg;

#if HDA_VERBOSE
		hda_dump_afg(afg);
#endif
		ANANAS_ERROR_RETURN(err);
	}

	return ANANAS_ERROR_OK;
}


/* vim:set ts=2 sw=2: */
