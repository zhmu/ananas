#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/bus/usb/config.h>
#include <ananas/bus/usb/core.h>
#include <ananas/bus/usb/pipe.h>
#include <ananas/bus/usb/transfer.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/thread.h>
#include <ananas/trace.h>
#include <ananas/time.h>
#include <ananas/waitqueue.h>
#include <machine/param.h>
#include "usb-hub.h"

TRACE_SETUP;

static usb_pipe_result_t
usbhub_int_callback(struct USB_PIPE* pipe)
{
	struct USB_TRANSFER* xfer = pipe->p_xfer;
	struct USB_DEVICE* usb_dev = pipe->p_dev;
	struct HUB_PRIVDATA* hub_privdata = usb_dev->usb_privdata;

	/* Error means nothing happened, so request a new transfer */
	if (xfer->xfer_flags & TRANSFER_FLAG_ERROR)
		return PIPE_OK;

	if (xfer->xfer_data[0] & 1) {
		/* Hub status changed - need to fetch the new status */
		hub_privdata->hub_flags |= HUB_FLAG_UPDATED;
	}

	for (int n = 1; n <= hub_privdata->hub_numports; n++) {
		if ((xfer->xfer_data[n / 8] & (1 << (n & 7))) == 0)
			continue;

		/* This port was updated - fetch the port status */
		hub_privdata->hub_port[n - 1].p_flags |= HUB_PORT_FLAG_UPDATED;
		kprintf("woke up hub port %u\n", n - 1);
	}

	kprintf("%s:%u: waking up thread\n", __func__, __LINE__);
	waitqueue_signal(&hub_privdata->hub_wq);
	return PIPE_OK;
}

static errorcode_t
usbhub_probe(device_t dev)
{
	struct USB_DEVICE* usb_dev = dev->parent->privdata;

	struct USB_INTERFACE* iface = &usb_dev->usb_interface[usb_dev->usb_cur_interface];
	if (iface->if_class != USB_IF_CLASS_HUB)
		return ANANAS_ERROR(NO_DEVICE);

	return ANANAS_ERROR_OK;
}

static void
usbhub_workerthread(void* ptr)
{
	struct USB_DEVICE* usb_dev = ptr;
	struct HUB_PRIVDATA* hub_privdata = usb_dev->usb_privdata;

	struct WAITER* w = waitqueue_add(&hub_privdata->hub_wq);
	while(1) {
		waitqueue_reset_waiter(w);
		waitqueue_wait(w);

		kprintf("%s:%u: woke up\n", __func__, __LINE__);

		/* Handle all ports that need handling */
		for (int n = 1; n <= hub_privdata->hub_numports; n++) {
			struct HUB_PORT* port = &hub_privdata->hub_port[n - 1];
			if ((port->p_flags & HUB_PORT_FLAG_UPDATED) == 0)
				continue;

		kprintf("%s:%u: port %u needs attention\n", __func__, __LINE__, n);

			struct HUB_PORT_STATUS ps;
			size_t len = sizeof(ps);
			errorcode_t err = usb_control_xfer(usb_dev, USB_CONTROL_REQUEST_GET_STATUS, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, HUB_FEATURE_PORT_POWER, n, &ps, &len, 0);
			if (err != ANANAS_ERROR_OK) {
				kprintf("hub returned error\n");
			}
			kprintf("port %u [%x,%x]\n", n, ps.ps_portstatus, ps.ps_portchange);
		}
	}
	waitqueue_remove(w);
}

static errorcode_t
usbhub_attach(device_t dev)
{
	struct USB_DEVICE* usb_dev = dev->parent->privdata;

	/* Obtain the hub descriptor */
	struct USB_DESCR_HUB hd;
	size_t len = sizeof(hd);
	errorcode_t err = usb_control_xfer(usb_dev, USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_CLASS, USB_REQUEST_MAKE(USB_DESCR_TYPE_HUB, 0), 0, &hd, &len, 0);
	ANANAS_ERROR_RETURN(err);

	struct HUB_PRIVDATA* hub_privdata = kmalloc(sizeof(*hub_privdata) + sizeof(struct HUB_PORT) * (hd.hd_numports - 1));
	memset(hub_privdata, 0, sizeof(*hub_privdata) + sizeof(struct HUB_PORT) * (hd.hd_numports - 1));
	waitqueue_init(&hub_privdata->hub_wq);
	usb_dev->usb_privdata = hub_privdata;
	hub_privdata->hub_numports = hd.hd_numports;
	device_printf(dev, "%u port(s)", hub_privdata->hub_numports);

	/* Enable power to all ports */
	for (int n = 0; n < hub_privdata->hub_numports; n++) {
		err = usb_control_xfer(usb_dev, USB_CONTROL_REQUEST_SET_FEATURE, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, HUB_FEATURE_PORT_POWER, n + 1, NULL, NULL, 1);
		if (err != ANANAS_ERROR_OK)
			goto fail;

		/* Wait until the power is good */
		delay(hd.hd_poweron2good * 2 + 10 /* slack */);
	}

	/* Hook up the hub thread; this is responsible for handling hub requests */
	kthread_init(&hub_privdata->hub_workerthread, &usbhub_workerthread, usb_dev);
	thread_set_args(&hub_privdata->hub_workerthread, "[usbhub]\0\0", PAGE_SIZE);
	thread_resume(&hub_privdata->hub_workerthread);

	/* Initialization went well; hook up the interrupt pipe so that we may receive updates */
	struct USB_PIPE* pipe;
	err = usb_pipe_alloc(usb_dev, 0, TRANSFER_TYPE_INTERRUPT, EP_DIR_IN, usbhub_int_callback, &pipe);
	if (err != ANANAS_ERROR_OK) {
		device_printf(dev, "endpoint 0 not interrupt/in");
		goto fail;
	}
	err = usb_pipe_start(pipe);
	if (err != ANANAS_ERROR_OK)
		goto fail;
	return err;

fail:
	kfree(hub_privdata);
	return err;
}

struct DRIVER drv_usbhub = {
	.name = "usbhub",
	.drv_probe = usbhub_probe,
	.drv_attach = usbhub_attach
};

DRIVER_PROBE(usbhub)
DRIVER_PROBE_BUS(usbdev)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
