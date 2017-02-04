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
#include <machine/param.h>
#include "usb-bus.h"
#include "usb-device.h"
#include "usb-hub.h"

#if 0
# define DPRINTF device_printf
#else
# define DPRINTF(...)
#endif

struct HUB_PORT {
	int	p_flags;
#define HUB_PORT_FLAG_CONNECTED		(1 << 0)		/* Device is connected */
#define HUB_PORT_FLAG_UPDATED		(1 << 1)		/* Port status is updated */
	struct USB_DEVICE* p_device;
};

struct USB_HUB {
	int		hub_flags;
#define HUB_FLAG_SELFPOWERED		(1 << 0)		/* Hub is self powered */
#define HUB_FLAG_UPDATED		(1 << 1)		/* Hub status needs updating */
	struct USB_DEVICE* hub_device;
	int		hub_numports;
	struct HUB_PORT	hub_port[0];
};

TRACE_SETUP;

static void
usbhub_int_callback(struct USB_PIPE* pipe)
{
	struct USB_TRANSFER* xfer = pipe->p_xfer;
	struct USB_DEVICE* usb_dev = pipe->p_dev;
	struct USB_HUB* usb_hub = usb_dev->usb_device->privdata;

	/* Error means nothing happened, so request a new transfer */
	if (xfer->xfer_flags & TRANSFER_FLAG_ERROR) {
		device_printf(usb_dev->usb_device, "usbhub_int_callback: error!");
		return;
	}

	if (xfer->xfer_data[0] & 1) {
		/* Hub status changed - need to fetch the new status */
		usb_hub->hub_flags |= HUB_FLAG_UPDATED;
	}

	DPRINTF(usb_dev->usb_device, "*** usbhub_int_callback(): changed %x", xfer->xfer_data[0]);

	int num_changed = 0;
	for (int n = 1; n <= usb_hub->hub_numports; n++) {
		if ((xfer->xfer_data[n / 8] & (1 << (n & 7))) == 0)
			continue;

		device_printf(usb_dev->usb_device, "port %d changed", n);

		/* This port was updated - fetch the port status */
		usb_hub->hub_port[n - 1].p_flags |= HUB_PORT_FLAG_UPDATED;
		num_changed++;
	}

	/* If anything was changed, we need to schedule a bus explore */
	if (num_changed > 0)
		usbbus_schedule_explore(usb_dev->usb_bus);

	/* Reschedule the pipe for future updates */
	usbpipe_schedule(pipe);
}

static errorcode_t
usbhub_probe(device_t dev)
{
	struct USB_DEVICE* usb_dev = device_alloc_resource(dev, RESTYPE_USB_DEVICE, 0);
	if (usb_dev == NULL)
		return ANANAS_ERROR(NO_DEVICE);

	struct USB_INTERFACE* iface = &usb_dev->usb_interface[usb_dev->usb_cur_interface];
	if (iface->if_class != USB_IF_CLASS_HUB)
		return ANANAS_ERROR(NO_DEVICE);

	return ananas_success();
}

errorcode_t
ushub_reset_port(struct USB_HUB* hub, int n)
{
	struct USB_DEVICE* hub_dev = hub->hub_device;
	KASSERT(n >= 1 && n <= hub->hub_numports, "port %d out of range", n);

	/* Reset the reset state of the port in case it lingers */
	DPRINTF(hub_dev->usb_device, "%s: port %d: clearing c_port_reset", __func__, n);
	errorcode_t err = usb_control_xfer(hub_dev, USB_CONTROL_REQUEST_CLEAR_FEATURE, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, HUB_FEATURE_C_PORT_RESET, n, NULL, NULL, 1);
	if (ananas_is_failure(err)) {
		device_printf(hub_dev->usb_device, "port_clear error %d, continuing anyway", err);
	}

	/* Need to reset the port */
	DPRINTF(hub_dev->usb_device, "%s: port %d: resetting", __func__, n);
	err = usb_control_xfer(hub_dev, USB_CONTROL_REQUEST_SET_FEATURE, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, HUB_FEATURE_PORT_RESET, n, NULL, NULL, 1);
	if (ananas_is_failure(err)) {
		device_printf(hub_dev->usb_device, "port_reset error %d, ignoring port", err);
		return err;
	}

	struct HUB_PORT_STATUS ps;
	int timeout = 10; /* XXX */
	while(timeout > 0) {
		delay(100);

		/* See if the device is correctly reset */
		size_t len = sizeof(ps);
		memset(&ps, 0, len);
		err = usb_control_xfer(hub_dev, USB_CONTROL_REQUEST_GET_STATUS, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, 0, n, &ps, &len, 0);
		if (ananas_is_failure(err)) {
			device_printf(hub_dev->usb_device, "get_status error %d, ignoring port", err);
			return err;
		}
	
		if ((ps.ps_portstatus & USB_HUB_PS_PORT_CONNECTION) == 0) {
			/* Port is no longer attached; give up */
			device_printf(hub_dev->usb_device, "port %d no longer connected, ignoring port", n);
			return err;
		}

		if (ps.ps_portchange & HUB_PORTCHANGE_RESET)
			break;

		timeout--;
	}
	if (timeout == 0) {
		device_printf(hub_dev->usb_device, "timeout resetting port %d", n);
		return ANANAS_ERROR(NO_DEVICE);
	}

	DPRINTF(hub_dev->usb_device, "%s: port %d: reset completed; clearing c_reset", __func__, n);
	err = usb_control_xfer(hub_dev, USB_CONTROL_REQUEST_CLEAR_FEATURE, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, HUB_FEATURE_C_PORT_RESET, n, NULL, NULL, 1);
	if (ananas_is_failure(err)) {
		device_printf(hub_dev->usb_device, "unable to clear reset of port %d", n);
		return err;
	}

	return ananas_success();
}

static void
usbhub_handle_explore_new_device(struct USB_DEVICE* hub_dev, struct HUB_PORT* port, int n)
{
	KASSERT(port->p_device == NULL, "exploring new device over current?");

	/* Fetch the port status; we need to know if it's lo- or high speed */
	struct HUB_PORT_STATUS ps;
	size_t len = sizeof(ps);
	memset(&ps, 0, len);
	errorcode_t err = usb_control_xfer(hub_dev, USB_CONTROL_REQUEST_GET_STATUS, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, 0, n, &ps, &len, 0);
	if (ananas_is_failure(err)) {
		device_printf(hub_dev->usb_device, "get_status(%d) error %d, ignoring port", n, err);
		return;
	}

	/* If the port is no longer connected, we needn't do anything */
	if ((ps.ps_portstatus & USB_HUB_PS_PORT_CONNECTION) == 0) {
		device_printf(hub_dev->usb_device, "port %d no longer connected, giving up", n);
		return;
	}

	/* Mark the port as attached */
	port->p_flags |= HUB_PORT_FLAG_CONNECTED;

	/* Hand it off to the USB framework */
	struct USB_HUB* hub_hub = hub_dev->usb_device->privdata;
	int flags = (ps.ps_portstatus & USB_HUB_PS_PORT_LOW_SPEED) != 0;
	port->p_device = usb_alloc_device(hub_dev->usb_bus, hub_hub, n, flags);
	usbbus_schedule_attach(port->p_device);
}

static void
usbhub_handle_detach(struct USB_DEVICE* hub_dev, struct HUB_PORT* port, int n)
{
	struct USB_DEVICE* usb_dev = port->p_device;
	KASSERT(usb_dev != NULL, "detaching null device");

	errorcode_t err = usbdev_detach(usb_dev);
	if (ananas_is_failure(err)) {
		device_printf(usb_dev->usb_device, "unable to detach device (%d)", err);
		return;
	}

	/* Deregister the device and mark the port as disconnected */
	port->p_device = NULL;
	port->p_flags &= ~HUB_PORT_FLAG_CONNECTED;
}

void
usbhub_handle_explore(struct USB_DEVICE* usb_dev)
{
	device_t dev = usb_dev->usb_device;
	struct USB_HUB* hub = dev->privdata;

	if (hub->hub_flags & HUB_FLAG_UPDATED) {
		device_printf(dev, "hub updated, todo");
		hub->hub_flags &= ~HUB_FLAG_UPDATED;
	}

	/* Handle all ports that need handling */
	for (int n = 1; n <= hub->hub_numports; n++) {
		struct HUB_PORT* port = &hub->hub_port[n - 1];
		if ((port->p_flags & HUB_PORT_FLAG_UPDATED) == 0)
			continue;

		struct HUB_PORT_STATUS ps;
		size_t len = sizeof(ps);
		errorcode_t err = usb_control_xfer(usb_dev, USB_CONTROL_REQUEST_GET_STATUS, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, 0, n, &ps, &len, 0);
		if (ananas_is_failure(err)) {
			device_printf(dev, "get_status error %d, ignoring port", err);
			continue;
		}

		if (ps.ps_portchange & HUB_PORTCHANGE_ENABLE) {
			DPRINTF(usb_dev->usb_device, "%s: port %d: enabled changed, clearing c_port_enable", __func__, n);
			usb_control_xfer(usb_dev, USB_CONTROL_REQUEST_CLEAR_FEATURE, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, HUB_FEATURE_C_PORT_ENABLE, n, NULL, NULL, 1);
		}

		if (ps.ps_portchange & HUB_PORTCHANGE_CONNECT) {
			/* Port connection changed - acknowledge this */
			DPRINTF(usb_dev->usb_device, "%s: port %d: connect changed, clearing c_port_connnection", __func__, n);
			usb_control_xfer(usb_dev, USB_CONTROL_REQUEST_CLEAR_FEATURE, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, HUB_FEATURE_C_PORT_CONNECTION, n, NULL, NULL, 1);

			if ((port->p_flags & HUB_PORT_FLAG_CONNECTED) == 0) {
				/* Nothing was connected to there; need to hook something up */
				usbhub_handle_explore_new_device(usb_dev, port, n);
			} else {
				usbhub_handle_detach(usb_dev, port, n);
			}
		}
	}
}

static errorcode_t
usbhub_attach(device_t dev)
{
	struct USB_DEVICE* usb_dev = device_alloc_resource(dev, RESTYPE_USB_DEVICE, 0);

	/* Obtain the hub descriptor */
	struct USB_DESCR_HUB hd;
	size_t len = sizeof(hd);
	errorcode_t err = usb_control_xfer(usb_dev, USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_CLASS, USB_REQUEST_MAKE(USB_DESCR_TYPE_HUB, 0), 0, &hd, &len, 0);
	ANANAS_ERROR_RETURN(err);

	struct USB_HUB* hub = kmalloc(sizeof(*hub) + sizeof(struct HUB_PORT) * hd.hd_numports);
	memset(hub, 0, sizeof(*hub) + sizeof(struct HUB_PORT) * hd.hd_numports);
	dev->privdata = hub;
	hub->hub_device = usb_dev;
	hub->hub_numports = hd.hd_numports;
	device_printf(dev, "%u port(s)", hub->hub_numports);

	/* Enable power to all ports */
	for (int n = 0; n < hub->hub_numports; n++) {
		err = usb_control_xfer(usb_dev, USB_CONTROL_REQUEST_SET_FEATURE, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, HUB_FEATURE_PORT_POWER, n + 1, NULL, NULL, 1);
		if (ananas_is_failure(err))
			goto fail;

		/* Force the port as 'updated' - we need to check it initially */
		hub->hub_port[n].p_flags = HUB_PORT_FLAG_UPDATED;

		/* Wait until the power is good */
		delay(hd.hd_poweron2good * 2 + 10 /* slack */);
	}

	/* Initialization went well; hook up the interrupt pipe so that we may receive updates */
	struct USB_PIPE* pipe;
	err = usbpipe_alloc(usb_dev, 0, TRANSFER_TYPE_INTERRUPT, EP_DIR_IN, 0, usbhub_int_callback, &pipe);
	if (ananas_is_failure(err)) {
		device_printf(dev, "endpoint 0 not interrupt/in");
		goto fail;
	}
	err = usbpipe_schedule(pipe);
	if (ananas_is_failure(err))
		goto fail;
	return err;

fail:
	kfree(hub);
	return err;
}

static errorcode_t
usbhub_detach(device_t dev)
{
	device_printf(dev, "usbhub_detach");

	struct USB_HUB* hub_hub = dev->privdata;
	usb_bus_detach_hub(hub_hub->hub_device->usb_bus, hub_hub);
	return ananas_success();
}

struct DRIVER drv_usbhub = {
	.name = "usbhub",
	.drv_probe = usbhub_probe,
	.drv_attach = usbhub_attach,
	.drv_detach = usbhub_detach,
	.drv_usb_explore = usbhub_handle_explore,
};

DRIVER_PROBE(usbhub)
DRIVER_PROBE_BUS(usbbus)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
