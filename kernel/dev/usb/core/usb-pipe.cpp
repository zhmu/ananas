#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/trace.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include "pipe.h"
#include "usb-device.h"
#include "usb-transfer.h"
#include "usb-core.h"

TRACE_SETUP;

namespace Ananas {
namespace USB {

namespace {

void
CallbackWrapper(Transfer& xfer)
{
	auto pipe = static_cast<Pipe*>(xfer.t_callback_data);
	kprintf("usbpipe_callback(): pipe=%p\n", pipe);
	pipe->p_callback.OnPipeCallback(*pipe);
}

static Transfer*
CreateTransfer(Pipe& pipe, size_t maxlen)
{
	Endpoint& ep = pipe.p_ep;
	Transfer* xfer = AllocateTransfer(pipe.p_dev, ep.ep_type, ((ep.ep_dir == EP_DIR_IN) ? TRANSFER_FLAG_READ : TRANSFER_FLAG_WRITE) | TRANSFER_FLAG_DATA, ep.ep_address, maxlen);
	xfer->t_length = ep.ep_maxpacketsize;
	xfer->t_callback = CallbackWrapper;
	xfer->t_callback_data = &pipe;
	return xfer;
}

} // unnamed namespace

errorcode_t
AllocatePipe(USBDevice& usb_dev, int num, int type, int dir, size_t maxlen, IPipeCallback& callback, Pipe*& pipe)
{
	KASSERT(dir == EP_DIR_IN || dir == EP_DIR_OUT, "invalid direction %u", dir);
	KASSERT(type == TRANSFER_TYPE_CONTROL || type == TRANSFER_TYPE_INTERRUPT || type == TRANSFER_TYPE_BULK || type == TRANSFER_TYPE_ISOCHRONOUS, "invalid type %u", type);

	Interface& uif = usb_dev.ud_interface[usb_dev.ud_cur_interface];
	if (num < 0 || num >= uif.if_num_endpoints)
		return ANANAS_ERROR(BAD_RANGE);

	Endpoint& ep = uif.if_endpoint[num];
	if (ep.ep_type != type || ep.ep_dir != dir)
		return ANANAS_ERROR(BAD_TYPE);

	if (maxlen == 0)
		maxlen = ep.ep_maxpacketsize;

	auto p = new Pipe(usb_dev, ep, callback);
	p->p_xfer = CreateTransfer(*p, maxlen);

	/* Hook up the pipe to the device */
	usb_dev.Lock();
	LIST_APPEND(&usb_dev.ud_pipes, p);
	usb_dev.Unlock();
	pipe = p;
	return ananas_success();
}

void
FreePipe(Pipe& pipe)
{
	USBDevice& usb_dev = pipe.p_dev;

	kprintf("FreePipe(): pipe=%p xfer=%p\n", &pipe, pipe.p_xfer);

	usb_dev.Lock();
	FreePipe_Locked(pipe);
	usb_dev.Unlock();
}

void
FreePipe_Locked(Pipe& pipe)
{
	USBDevice& usb_dev = pipe.p_dev;
	usb_dev.AssertLocked();

	/* Free the transfer, if we one exists (this cancels it as well) */
	if (pipe.p_xfer != nullptr)
		FreeTransfer_Locked(*pipe.p_xfer);

	/* Unregister the pipe - this is where we need the lock for */
	LIST_REMOVE(&usb_dev.ud_pipes, &pipe);
	delete &pipe;
}

errorcode_t
SchedulePipe(Pipe& pipe)
{
	kprintf("usbpipe_schedule(): pipe=%p xfer=%p\n", &pipe, &pipe.p_xfer);
	return ScheduleTransfer(*pipe.p_xfer);
}

} // namespace USB
} // namespace Ananas


/* vim:set ts=2 sw=2: */
