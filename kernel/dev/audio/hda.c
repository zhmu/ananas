#include <ananas/types.h>
#include <ananas/dev/hda.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/trace.h>
#include <machine/param.h>
#include "hda.h"

#include <ananas/dev/hda-pci.h> // XXX
#include <ananas/thread.h>

TRACE_SETUP;

#define DEMO_PLAY 0

#if DEMO_PLAY
static unsigned int cur_stream_offset = 0;
static unsigned int cur_buffer = 0;

static uint8_t* play_buf = NULL;
static unsigned int play_buf_size = (64 * PAGE_SIZE);
static unsigned int play_buf_filled = 0;
static semaphore_t play_sem;
static mutex_t play_mtx;


void sys_play(thread_t* t, const void* buf, size_t len)
{
	mutex_lock(&play_mtx);
	while(len > 0) {
		unsigned int size_left = play_buf_size - play_buf_filled;
		if (size_left == 0) {
			/* No space left in our play buffer -> block */
			mutex_unlock(&play_mtx);
			sem_wait_and_drain(&play_sem);
			mutex_lock(&play_mtx);
		}

		/* Copy as much to our play buffer as we can */
		unsigned int chunk_len = len;
		if (chunk_len > size_left)
			chunk_len = size_left;
		memcpy(&play_buf[play_buf_filled], buf, chunk_len);
		play_buf_filled += chunk_len;
		len -= chunk_len;
	}
	mutex_unlock(&play_mtx);
}
#endif

void
hda_stream_irq(device_t dev, void* ctx)
{
#if DEMO_PLAY
	struct HDA_PRIVDATA* privdata = dev->privdata;
	struct HDA_DEV_FUNC* devfuncs = privdata->hda_dev_func;
	struct HDA_PCI_STREAM* s = ctx;

	/* Fill the other half of the play buffer with new data */
	int num_pages = s->s_num_pages;
	for (unsigned int n = 0; n < num_pages / 2; n++) {
		uint8_t* ptr = devfuncs->hdf_get_stream_data_ptr(dev, ctx, n + (cur_buffer * num_pages / 2));

		mutex_lock(&play_mtx);
		if (play_buf_filled > 0) {
			int chunk_len = PAGE_SIZE;
			if (chunk_len > play_buf_filled)
				chunk_len = play_buf_filled;
			memcpy(ptr, play_buf, chunk_len);
			memmove(&play_buf[0], &play_buf[chunk_len], play_buf_filled - chunk_len);
			play_buf_filled -= chunk_len;

			/* Unblock anyone attempting to fill the buffer, we have spots now */
			sem_signal(&play_sem);
		}
		mutex_unlock(&play_mtx);
	}

	cur_buffer ^= 1;
#endif
}

errorcode_t
hda_attach(device_t dev, struct HDA_DEV_FUNC* devfuncs, void* dev_privdata)
{
	struct HDA_PRIVDATA* privdata = kmalloc(sizeof *privdata);
	dev->privdata = privdata;
	memset(privdata, 0, sizeof(*privdata));
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

#if DEMO_PLAY
	struct HDA_AFG* afg = privdata->hda_afg;
	if (afg == NULL || LIST_EMPTY(&afg->afg_outputs))
		return ANANAS_ERROR(NO_DEVICE); /* XXX can this happen? */

	/* Let's play something! */
	struct HDA_OUTPUT* o = NULL;
	LIST_FOREACH(&afg->afg_outputs, ao, struct HDA_OUTPUT) {
		/* XXX We specifically look for a 2-channel only output - this is wrong, but VirtualBox
		 *     dies if we try to use 2-channels for a 7.1-output. Need to look into this XXX
		 */
		if (ao->o_channels == 2) {
			o = ao;
			break;
		}
	}
	if (o == NULL) {
		kprintf("cannot find 2-channel output, aborting\n");
		return ananas_success();
	}

	struct HDA_ROUTING_PLAN* rp;
	errorcode_t err = hda_route_output(dev, afg, o->o_channels, o, &rp);
	if (ananas_is_failure(err))
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

		/* set stream format: 48.0Hz, 16-bit stereo  */
		int stream_fmt = STREAM_TYPE_PCM | STREAM_BASE_48_0 | STREAM_MULT_x1 | STREAM_DIV_1 | STREAM_BITS_16 | STREAM_CHANNELS(1);
		err = devfuncs->hdf_issue_verb(dev, HDA_MAKE_VERB_NODE(ao, HDA_MAKE_PAYLOAD_ID4(HDA_CODEC_CMD_SET_CONV_FORMAT, stream_fmt)), &r, NULL);
		ANANAS_ERROR_RETURN(err);

		/* open a stream to play things */
		void* ctx;
		int num_pages = 64;
		err = devfuncs->hdf_open_stream(dev, tag, HDF_DIR_OUT, stream_fmt, num_pages, &ctx);
		ANANAS_ERROR_RETURN(err);

		/* Fill the stream with silence */
		cur_stream_offset = 0;
		for (unsigned int n = 0; n < num_pages; n++) {
			uint8_t* ptr = devfuncs->hdf_get_stream_data_ptr(dev, ctx, n);
			for (unsigned int m = 0; m < PAGE_SIZE; m++) {
				*ptr++ = 0x80;
			}
		}
		contexts[num_contexts++] = ctx;
	}

	play_buf = kmalloc(PAGE_SIZE * 64);
	memset(play_buf, 0x80, PAGE_SIZE * 64);

	mutex_init(&play_mtx, "play");
	sem_init(&play_sem, 0);

	err = devfuncs->hdf_start_streams(dev, num_contexts, contexts);
	ANANAS_ERROR_RETURN(err);
#endif
	return ananas_success();
}

/* vim:set ts=2 sw=2: */
