#include <ananas/types.h>
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "hda.h"

#include <sys/sound.h>
#include <machine/param.h>
#include "hda-pci.h" // XXX

TRACE_SETUP;

namespace Ananas {
namespace HDA {

struct PlayContext {
	PlayContext(IHDAFunctions& hda_funcs, int buffer_len_in_pages)
	 : hdaFunctions(hda_funcs)
	{
			pc_buffer_length = buffer_len_in_pages * PAGE_SIZE;
			pc_play_buf = static_cast<uint8_t*>(kmalloc(pc_buffer_length));
			memset(pc_play_buf, 0x80, pc_buffer_length);
	}

	~PlayContext()
	{
		if (auto result = Stop(); result.IsFailure())
			panic("cannot deal with failure %d here!", result.AsStatusCode());

		for (int n = 0; n < pc_num_contexts; n++) {
			if (auto result = hdaFunctions.CloseStream(pc_contexts[n]); result.IsFailure())
				panic("cannot deal with failure %d here!", result.AsStatusCode());
		}
		kfree(pc_play_buf);
	}

	Result Start()
	{
		return hdaFunctions.StartStreams(pc_num_contexts, pc_contexts);
	}

	Result Stop()
	{
		return hdaFunctions.StopStreams(pc_num_contexts, pc_contexts);
	}

	unsigned int pc_buffer_length;
	unsigned int pc_buffer_filled = 0;
	unsigned int pc_stream_offset = 0;
	unsigned int pc_cur_buffer = 0;
	IHDAFunctions::Context pc_contexts[10] = {}; // XXX why 10
	int pc_num_contexts = 0;
	uint8_t* pc_play_buf = nullptr;
	Mutex pc_mutex{"play"};
	Semaphore pc_semaphore{1};
	IHDAFunctions& hdaFunctions;
};

RoutingPlan::~RoutingPlan()
{
	delete rp_node;
}

PinGroup::~PinGroup()
{
	delete pg_pin;
}

void
HDADevice::OnStreamIRQ(IHDAFunctions::Context ctx)
{
	auto s = static_cast<HDA_PCI_STREAM*>(ctx);
	KASSERT(hda_pc != nullptr, "stream irq without playing");
	auto& pc = *hda_pc;

	// Fill the other half of the play buffer with new data
	int num_pages = s->s_num_pages;
	for (unsigned int n = 0; n < num_pages / 2; n++) {
		auto ptr = static_cast<uint8_t*>(hdaFunctions->GetStreamDataPointer(ctx, n + (pc.pc_cur_buffer * num_pages / 2)));

		{
			MutexGuard g(pc.pc_mutex);
			if (pc.pc_buffer_filled > 0) {
				int chunk_len = PAGE_SIZE;
				if (chunk_len > pc.pc_buffer_filled)
					chunk_len = pc.pc_buffer_filled;
				memcpy(ptr, pc.pc_play_buf, chunk_len);
				memmove(&pc.pc_play_buf[0], &pc.pc_play_buf[chunk_len], pc.pc_buffer_filled - chunk_len);
				pc.pc_buffer_filled -= chunk_len;

				// Unblock anyone attempting to fill the buffer, we have spots now
				pc.pc_semaphore.Signal();
			}
		}
	}

	// Next time, update the next buffer
	pc.pc_cur_buffer ^= 1;
}

Result
HDADevice::Attach()
{
	KASSERT(hdaFunctions != nullptr, "HDADevice::Attach() without hdaFunctions!");

	uint16_t codec_mask = hdaFunctions->GetAndClearStateChange();
	if (codec_mask == 0) {
		Printf("no codecs present, aborting attach");
		return RESULT_MAKE_FAILURE(ENODEV);
	}

	for (unsigned int cad = 0; cad <= 15; cad++) {
		if ((codec_mask & (1 << cad)) == 0)
			continue; // no codec on this address

		/* Attach the root node - it will recurse to subnodes if needed */
		RESULT_PROPAGATE_FAILURE(
			AttachNode(cad, 0)
		);
	}

	return Result::Success();
}

Result
HDADevice::Detach()
{
	panic("detach");
	return Result::Success();
}

Result
HDADevice::Write(const void* buffer, size_t len, off_t offset)
{
	if (hda_pc == nullptr)
		return RESULT_MAKE_FAILURE(EIO);
	auto& pc = *hda_pc;

	// Copy buffer content to what we will play
	pc.pc_mutex.Lock();
	size_t written = 0, left = len;
	auto buf = static_cast<const char*>(buffer);
	while(left > 0) {
		auto size_left = pc.pc_buffer_length - pc.pc_buffer_filled;
		if (size_left == 0) {
			// No space left in our play buffer -> block
			pc.pc_mutex.Unlock();
			pc.pc_semaphore.WaitAndDrain();
			pc.pc_mutex.Lock();
			continue;
		}

		// Copy as much to our play buffer as we can
		auto chunk_len = left;
		if (chunk_len > size_left)
			chunk_len = size_left;
		memcpy(&pc.pc_play_buf[pc.pc_buffer_filled], buf, chunk_len);

		// And advance the buffer
		pc.pc_buffer_filled += chunk_len;
		buf += chunk_len;
		left -= chunk_len;
		written += chunk_len;
	}
	pc.pc_mutex.Unlock();

	return Result::Success(written);
}

Result
HDADevice::Read(void* buffer, size_t len, off_t offset)
{
	// No recording support yet
	return RESULT_MAKE_FAILURE(EIO);
}

Result
HDADevice::IOControl(Process* proc, unsigned long req, void* args[])
{
	switch(req) {
		case IOCTL_SOUND_START: {
			auto ss = (SOUND_START_ARGS*)args[0]; // XXX userland check
			// For now, we only support a very limited subset...
			if (ss->ss_rate != SOUND_RATE_48KHZ)
				return RESULT_MAKE_FAILURE(EINVAL);
			if (ss->ss_format != SOUND_FORMAT_16S)
				return RESULT_MAKE_FAILURE(EINVAL);
			if (ss->ss_channels != 2)
				return RESULT_MAKE_FAILURE(EINVAL);
			if (ss->ss_buffer_length != 64)
				return RESULT_MAKE_FAILURE(EINVAL);

			// Only allow a single play context
			if (hda_pc != nullptr)
				return RESULT_MAKE_FAILURE(EPERM);

			AFG* afg = hda_afg;
			if (afg == NULL || afg->afg_outputs.empty())
				return RESULT_MAKE_FAILURE(EIO); // XXX could this happen?

			// XXX Look for a precisely N-channel output. This should not be necessary,
			// but VirtualBox dies if we try to use 2 channels only for a 7.1-channel output...
			auto o = [](AFG& afg, SOUND_START_ARGS& ss) -> Output* {
				for(auto& ao: afg.afg_outputs) {
					if (ao.o_channels == ss.ss_channels)
						return &ao;
				}
				return nullptr;
			}(*afg, *ss);
			if (o == nullptr)
				return RESULT_MAKE_FAILURE(EIO);

			RoutingPlan* rp;
			if (auto result = RouteOutput(*afg, o->o_channels, *o, &rp); result.IsFailure())
				return result;

			// Set up the play context; we'll be filling out streams shortly
			hda_pc = new PlayContext(*hdaFunctions, ss->ss_buffer_length);

			int tag = 1; // XXX Why?
			for (int n = 0; n < rp->rp_num_nodes; n++) {
				if (rp->rp_node[n]->GetType() != NT_AudioOut)
					continue;
				auto& ao = static_cast<Node_AudioOut&>(*rp->rp_node[n]);

				// Hook streams up
				uint32_t r;
				if (auto result = hdaFunctions->IssueVerb(HDA_MAKE_VERB_NODE(ao, HDA_MAKE_PAYLOAD_ID12(HDA_CODEC_CMD_SETCONVCONTROL, HDA_CODEC_CONVCONTROL_STREAM(tag) | HDA_CODEC_CONVCONTROL_CHANNEL(0))), &r, NULL); result.IsFailure())
					return result;

				// Set stream format: 48.0Hz, 16-bit stereo
				int stream_fmt = STREAM_TYPE_PCM | STREAM_BASE_48_0 | STREAM_MULT_x1 | STREAM_DIV_1 | STREAM_BITS_16 | STREAM_CHANNELS(1);
				{
					RESULT_PROPAGATE_FAILURE(
						hdaFunctions->IssueVerb(HDA_MAKE_VERB_NODE(ao, HDA_MAKE_PAYLOAD_ID4(HDA_CODEC_CMD_SET_CONV_FORMAT, stream_fmt)), &r, NULL)
					);
				}

				// open a stream to play things
				IHDAFunctions::Context ctx;
				int num_pages = ss->ss_buffer_length;
				RESULT_PROPAGATE_FAILURE(
					hdaFunctions->OpenStream(tag, HDF_DIR_OUT, stream_fmt, num_pages, &ctx)
				);

				// Fill the stream with silence
				for (unsigned int n = 0; n < num_pages; n++) {
					auto ptr = static_cast<uint8_t*>(hdaFunctions->GetStreamDataPointer(ctx, n));
					for (unsigned int m = 0; m < PAGE_SIZE; m++) {
						*ptr++ = 0x80;
					}
				}
				hda_pc->pc_contexts[hda_pc->pc_num_contexts++] = ctx;
				tag++;
			}

			// Let there be sound!
			if (auto result = hda_pc->Start(); result.IsFailure()) {
				delete hda_pc;
				hda_pc = nullptr;
				return result;
			}
			return Result::Success();
		}

		case IOCTL_SOUND_STOP: {
			if (hda_pc == nullptr)
				return RESULT_MAKE_FAILURE(EPERM);

			delete hda_pc;
			hda_pc = nullptr;
			return Result::Success();
		}
	}
	return RESULT_MAKE_FAILURE(EIO);
}

Result
HDADevice::Open(Process* proc)
{
	// If we have a play context, reject the open
	if (hda_pc != nullptr)
		return RESULT_MAKE_FAILURE(EPERM);

	return Result::Success();
}

Result
HDADevice::Close(Process* proc)
{
	// Kill the play context; it belongs to whoever opened us
	delete hda_pc;
	hda_pc = nullptr;
	return Result::Success();
}

namespace {

struct HDA_Driver : public Ananas::Driver
{
	HDA_Driver()
	 : Driver("hda")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return nullptr; // created by hda-pci
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		return new HDADevice(cdp);
	}
};

} // unnamed namespace

REGISTER_DRIVER(HDA_Driver)

} // namespace HDA
} // namespace Ananas

/* vim:set ts=2 sw=2: */
