#include <ananas/types.h>
#include <ananas/dev/hda.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/trace.h>
#include <machine/param.h>
#include "hda.h"

TRACE_SETUP;

errorcode_t
hda_attach(device_t dev, struct HDA_DEV_FUNC* devfuncs, void* dev_privdata)
{
	struct HDA_PRIVDATA* privdata = kmalloc(sizeof *privdata);
	dev->privdata = privdata;
	memset(privdata, 0, sizeof(privdata));
	privdata->hda_dev_priv = dev_privdata;
	privdata->hda_dev_func = devfuncs;

	uint16_t codec_mask = devfuncs->hdf_get_state_change(dev);
	if (codec_mask == 0) {
		device_printf(dev, "no codecs present, aborting attach");
		return ANANAS_ERROR(NO_DEVICE);
	}

	for (unsigned int cad = 0; cad <= 15; cad++) {
		if ((codec_mask & (1 << cad)) == 0)
			continue; // no codec on this address

		/* Attach the root node - it will recurse to subnodes if needed */
		errorcode_t err = hda_attach_node(dev, cad, 0);		
		ANANAS_ERROR_RETURN(err);
	}

	/*
	 * Below this is debug code to play things
	 */

	struct HDA_AFG* afg = privdata->hda_afg;
	if (afg == NULL || DQUEUE_EMPTY(&afg->afg_outputs))
		return ANANAS_ERROR(NO_DEVICE); /* XXX can this happen? */

	/* Let's play something! */
	struct HDA_OUTPUT* o = DQUEUE_HEAD(&afg->afg_outputs);
	struct HDA_ROUTING_PLAN* rp;
	errorcode_t err = hda_route_output(dev, afg, 8, o, &rp);
	if (err != ANANAS_ERROR_OK)
		return err;

	void* contexts[10]; /* XXX */
	int num_contexts = 0;

	int tag = 0;
	for (int n = 0; n < rp->rp_num_nodes; n++) {
		struct HDA_NODE_AUDIOOUT* ao = (struct HDA_NODE_AUDIOOUT*)rp->rp_node[n];
		if (ao->ao_node.aw_node.n_type != NT_AudioOut)
			continue;

		tag++; /* XXX */

		/* hook stream up to output 2 */
		uint32_t r;
		err = devfuncs->hdf_issue_verb(dev, HDA_MAKE_VERB_NODE(ao, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_SETCONVCONTROL,
		 HDA_CODEC_CONVCONTROL_STREAM(tag) | HDA_CODEC_CONVCONTROL_CHANNEL(0)
		)), &r, NULL);
		ANANAS_ERROR_RETURN(err);

		/* set stream format */
		int stream_fmt = STREAM_TYPE_PCM | STREAM_BASE_48_0 | STREAM_MULT_x1 | STREAM_DIV_1 | STREAM_BITS_16 | STREAM_CHANNELS(1);
		err = devfuncs->hdf_issue_verb(dev, HDA_MAKE_VERB_NODE(ao, HDA_MAKE_PAYLOAD_ID4(HDA_CODEC_CMD_SET_CONV_FORMAT, stream_fmt)), &r, NULL);
		ANANAS_ERROR_RETURN(err);

		void* ctx;
		int num_pages = 4;
		err = devfuncs->hdf_open_stream(dev, tag, HDF_DIR_OUT, stream_fmt, num_pages, &ctx);
		ANANAS_ERROR_RETURN(err);

		/* Fill the stream with some data! */
		for (unsigned int n = 0; n < PAGE_SIZE * num_pages; n++) {
			uint8_t* ptr = devfuncs->hdf_get_stream_data_ptr(dev, ctx, n / PAGE_SIZE);
			ptr += n % PAGE_SIZE;
			if (num_contexts & 1)
				*ptr = (uint8_t)(n & 0xff);
			else
				*ptr = (uint8_t)(n & 0x7f);
		}
		contexts[num_contexts++] = ctx;
	}

	kprintf("starting streams, num=%d\n", num_contexts);

	err = devfuncs->hdf_start_streams(dev, num_contexts, contexts);
	ANANAS_ERROR_RETURN(err);
		
	return ANANAS_ERROR_NONE;
}

/* vim:set ts=2 sw=2: */
