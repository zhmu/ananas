#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/device.h"
#include "kernel/lib.h"
#include "kernel/list.h"
#include "kernel/mm.h"
#include "kernel/init.h"
#include "kernel/thread.h"
#include "config.h"
#include "usb-bus.h"
#include "usb-core.h"
#include "usb-device.h"
#include "usb-transfer.h"

#if 0
# define DPRINTF kprintf
#else
# define DPRINTF(...)
#endif

namespace Ananas {
namespace USB {

namespace {

Thread usbtransfer_thread;
semaphore_t usbtransfer_sem;
TransferQueue usbtransfer_complete;
spinlock_t usbtransfer_lock = SPINLOCK_DEFAULT_INIT;

inline Device&
usbtransfer_get_hcd_device(Transfer& xfer)
{
	/* All USB_DEVICE's are on usbbusX; the bus' parent is the HCD to use */
	return *xfer.t_device.ud_bus.d_Parent;
}

errorcode_t
SetupTransfer(Transfer& xfer)
{
	auto& hcd_dev = usbtransfer_get_hcd_device(xfer);
	KASSERT(hcd_dev.GetUSBDeviceOperations() != NULL, "transferring without usb transfer");
	return hcd_dev.GetUSBDeviceOperations()->SetupTransfer(xfer);
}

} // unnamed namespace

Transfer*
AllocateTransfer(USBDevice& usb_dev, int type, int flags, int endpt, size_t maxlen)
{
	auto usb_xfer = new Transfer(usb_dev, type, flags);
	DPRINTF("usbtransfer_alloc: xfer=%x type %d\n", usb_xfer, type);
	usb_xfer->t_length = maxlen; /* transfer buffer length */
	usb_xfer->t_address = usb_dev.ud_address;
	usb_xfer->t_endpoint = endpt;
	sem_init(&usb_xfer->t_semaphore, 0);

	errorcode_t err = SetupTransfer(*usb_xfer);
	if (ananas_is_failure(err)) {
		kfree(usb_xfer);
		return NULL;
	}
	return usb_xfer;
}

errorcode_t
Transfer::Schedule()
{
	/* All USB_DEVICE's are on usbbusX; the bus' parent is the HCD to use */
	auto& hcd_dev = usbtransfer_get_hcd_device(*this);

	/* Schedule the transfer; we are responsible for locking here */
	t_device.Lock();
	errorcode_t err = hcd_dev.GetUSBDeviceOperations()->ScheduleTransfer(*this);
	t_device.Unlock();
	return err;
}

errorcode_t
Transfer::Cancel_Locked()
{
	auto& hcd_dev = usbtransfer_get_hcd_device(*this);
	t_device.AssertLocked();

	// We have to ensure the transfer isn't in the completed queue; this would cause it to be re-scheduled,
	// which we must avoid XXX We already have the device lock here - is that wise?
	spinlock_lock(&usbtransfer_lock);
	LIST_FOREACH_IP(&usbtransfer_complete, completed, transfer, Transfer) {
		if (transfer != this)
			continue;

		LIST_REMOVE_IP(&usbtransfer_complete, completed, this);
		break;
	}
	spinlock_unlock(&usbtransfer_lock);

	// XXX how do we know the transfer thread wasn't handling _this_ completed transfer by here?
	return hcd_dev.GetUSBDeviceOperations()->CancelTransfer(*this);
}

errorcode_t
Transfer::Cancel()
{
	t_device.Lock();
	errorcode_t err = Cancel_Locked();
	t_device.Unlock();
	return err;
}

void
FreeTransfer_Locked(Transfer& xfer)
{
	auto& usb_dev = xfer.t_device;
	auto& hcd_dev = usbtransfer_get_hcd_device(xfer);
	usb_dev.AssertLocked();

	xfer.Cancel_Locked();
	hcd_dev.GetUSBDeviceOperations()->TearDownTransfer(xfer);
	delete &xfer;
}

void
FreeTransfer(Transfer& xfer)
{
	auto& usb_dev = xfer.t_device;

	DPRINTF("usbtransfer_free: xfer=%x type %d\n", xfer, xfer->t_type);
	usb_dev.Lock();
	FreeTransfer_Locked(xfer);
	usb_dev.Unlock();
}

void
Transfer::Complete_Locked()
{
	t_device.AssertLocked();
	KASSERT(t_flags & TRANSFER_FLAG_PENDING, "completing transfer that isn't pending");

	/* Transfer is complete, so we can remove the pending flag */
	t_flags &= ~TRANSFER_FLAG_PENDING;
	LIST_REMOVE_IP(&t_device.ud_transfers, pending, this);

	/*
	 * This is generally called from interrupt context, so schedule a worker to
	 * process the transfer; if the transfer doesn't have a callback function,
	 * assume we'll just have to signal its semaphore.
	 */
	if (t_callback != nullptr) {
		spinlock_lock(&usbtransfer_lock);
		LIST_APPEND_IP(&usbtransfer_complete, completed, this);
		spinlock_unlock(&usbtransfer_lock);

		sem_signal(&usbtransfer_sem);
	} else {
		sem_signal(&t_semaphore);
	}
}

void
Transfer::Complete()
{
	t_device.Lock();
	Complete_Locked();
	t_device.Unlock();
}

namespace {

void
transfer_thread(void* arg)
{
	while(1) {
		/* Wait until there's something to report */
		sem_wait(&usbtransfer_sem);

		/* Fetch an entry from the queue */
		while (1) {
			spinlock_lock(&usbtransfer_lock);
			if(LIST_EMPTY(&usbtransfer_complete)) {
				spinlock_unlock(&usbtransfer_lock);
				break;
			}
			Transfer* xfer = LIST_HEAD(&usbtransfer_complete);
			LIST_POP_HEAD_IP(&usbtransfer_complete, completed);
			spinlock_unlock(&usbtransfer_lock);

			/* And handle it */
			KASSERT(xfer->t_callback != nullptr, "xfer %p in completed list without callback?", xfer);
			xfer->t_callback(*xfer);

			/*
			 * At this point, xfer may be freed, resubmitted or simply left lingering for
			 * the caller - we can't touch it at this point.
			 */
		}
	}
}

errorcode_t
InitializeTransfer()
{
	LIST_INIT(&usbtransfer_complete);
	sem_init(&usbtransfer_sem, 0);

	/* Create a kernel thread to handle USB completed messages */
	kthread_init(usbtransfer_thread, "usb-transfer", &transfer_thread, NULL);
	thread_resume(usbtransfer_thread);
	return ananas_success();
}

} // unnamed namespace

INIT_FUNCTION(InitializeTransfer, SUBSYSTEM_DEVICE, ORDER_FIRST);

} // namespace USB
} // namespace Ananas

/* vim:set ts=2 sw=2: */
