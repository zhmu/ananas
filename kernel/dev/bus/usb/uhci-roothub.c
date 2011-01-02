/*
 * UHCI root hub
 */
#include <ananas/types.h>
#include <ananas/bus/pci.h>
#include <ananas/bus/usb/descriptor.h>
#include <ananas/bus/usb/core.h>
#include <ananas/x86/io.h>
#include <ananas/error.h>
#include <ananas/trace.h>
#include <ananas/thread.h>
#include <ananas/schedule.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <machine/param.h> /* for PAGE_SIZE */
#include "uhci-reg.h"
#include "uhci-hcd.h"
#include "uhci-roothub.h"

TRACE_SETUP;

static errorcode_t
uroothub_ctrl_xfer(device_t dev, struct USB_TRANSFER* xfer)
{
	struct USB_CONTROL_REQUEST* req = &xfer->xfer_control_req;
	struct UHCI_HUB_PRIVDATA* hub_privdata = dev->privdata;
	struct UHCI_HCD_PRIVDATA* hcd_privdata = hub_privdata->hub_uhcidev->privdata;
	int port_io = hcd_privdata->uhci_io + UHCI_REG_PORTSC1 + (req->req_index - 1) * 2;
	errorcode_t err = ANANAS_ERROR_OK;

	switch(USB_REQUEST_MAKE(req->req_type, req->req_request)) {
		case USB_REQUEST_CLEAR_HUB_FEATURE:
		case USB_REQUEST_SET_HUB_FEATURE:
			/* Setting/clearing the supported hub features is a no-op; everything else is an error */
			if (req->req_value != HUB_FEATURE_C_HUB_LOCAL_POWER &&
			    req->req_value != HUB_FEATURE_C_HUB_OVER_CURRENT)
				err = ANANAS_ERROR(BAD_TYPE);
			break;
		case USB_REQUEST_CLEAR_PORT_FEATURE: {
			uint32_t clear_bit = 0;
			switch (req->req_value) {
				case HUB_FEATURE_PORT_ENABLE:
					clear_bit = UHCI_PORTSC_PORTEN;
					break;
				case HUB_FEATURE_PORT_SUSPEND:
					/* XXX not supported */
					break;
				case HUB_FEATURE_PORT_POWER:
					break;
				case HUB_FEATURE_C_PORT_CONNECTION:
					clear_bit = UHCI_PORTSC_CSCHANGE;
					break;
				case HUB_FEATURE_C_PORT_RESET:
					/* Can't report them, UHCI doesn't log them */
					break;
				case HUB_FEATURE_C_PORT_ENABLE:
					clear_bit = UHCI_PORTSC_PECHANGE;
					break;
				case HUB_FEATURE_C_PORT_SUSPEND:
					/* XXX not supported */
					break;
				case HUB_FEATURE_C_PORT_OVER_CURRENT:
					/* XXX not supported */
					break;
				default:
					err = ANANAS_ERROR(BAD_TYPE);
			}

			if (clear_bit) {
				outw(port_io, (inw(port_io) & (UHCI_PORTSC_SUSP | UHCI_PORTSC_RESET | UHCI_PORTSC_RD | UHCI_PORTSC_PORTEN)) & ~clear_bit);
			}
			break;
		}
		case USB_REQUEST_GET_BUS_STATE: {
			int linestat = inw(port_io);
			xfer->xfer_data[0] = 0; xfer->xfer_result_length = 1;
			if (linestat & UHCI_PORTSC_LS_DMINUS) xfer->xfer_data[0] |= (1 << 0);
			if (linestat & UHCI_PORTSC_LS_DPLUS)  xfer->xfer_data[0] |= (1 << 1);
			break;
		}
		case USB_REQUEST_GET_HUB_DESCRIPTOR:
			/* XXX TODO */
			break;
		case USB_REQUEST_GET_HUB_STATUS:
			if (req->req_value != 0 || req->req_index != 0 || req->req_length != 4) {
				err = ANANAS_ERROR(BAD_RANGE);
			} else {
				/* power good */
				xfer->xfer_data[0] = 0; xfer->xfer_data[1] = 0;
				xfer->xfer_data[2] = 0; xfer->xfer_data[3] = 0;
				xfer->xfer_result_length = 4;
			}
			break;
		case USB_REQUEST_GET_PORT_STATUS: {
			struct USB_HUB_PORTSTATUS* ps = (struct USB_HUB_PORTSTATUS*)xfer->xfer_data;
			xfer->xfer_result_length = sizeof(struct USB_HUB_PORTSTATUS);
			ps->ps_portstatus = USB_HUB_PS_PORT_POWER; /* always powered */
			ps->ps_portchange = 0;

			int portstat = inw(port_io);
			if (portstat & UHCI_PORTSC_SUSP)     ps->ps_portstatus |= USB_HUB_PS_PORT_SUSPEND;
			if (portstat & UHCI_PORTSC_RESET)    ps->ps_portstatus |= USB_HUB_PS_PORT_RESET;
			if (portstat & UHCI_PORTSC_LOWSPEED) ps->ps_portstatus |= USB_HUB_PS_PORT_LOW_SPEED;
			if (portstat & UHCI_PORTSC_PECHANGE) ps->ps_portchange |= USB_HUB_PC_C_PORT_ENABLE;
			if (portstat & UHCI_PORTSC_PORTEN)   ps->ps_portstatus |= USB_HUB_PS_PORT_ENABLE;
			if (portstat & UHCI_PORTSC_CSCHANGE) ps->ps_portchange |= USB_HUB_PC_C_PORT_CONNECTION;
			if (portstat & UHCI_PORTSC_CONNSTAT) ps->ps_portstatus |= USB_HUB_PS_PORT_CONNECTION;
			/* XXX  No support for overcurrent/suspend stuff */
			/* NOTE No support for reset change */
			break;
		}
		case USB_REQUEST_SET_PORT_FEATURE: {
			uint32_t set_bit = 0;
			switch (req->req_value) {
				case HUB_FEATURE_PORT_RESET:
					set_bit = UHCI_PORTSC_RESET;
					break;
				case HUB_FEATURE_PORT_SUSPEND:
					/* XXX */
					break;
				case HUB_FEATURE_PORT_POWER:
					/* UHCI doesn't support per-port power */
					break;
				default:
					/* We don't support anything else */
					err = ANANAS_ERROR(BAD_TYPE);
			}
			if (set_bit) {
				outw(port_io, (inw(port_io) & (UHCI_PORTSC_SUSP | UHCI_PORTSC_RESET | UHCI_PORTSC_RD | UHCI_PORTSC_PORTEN)) | set_bit);
			}
			break;
		}
		default:
			err = ANANAS_ERROR(BAD_TYPE);
			break;
	}

	return err;
}

static errorcode_t
uroothub_xfer(device_t dev, struct USB_TRANSFER* xfer)
{
	struct USB_CONTROL_REQUEST* req = &xfer->xfer_control_req;
	struct UHCI_HUB_PRIVDATA* hub_privdata = dev->privdata;
	struct UHCI_HCD_PRIVDATA* hcd_privdata = hub_privdata->hub_uhcidev->privdata;

	switch(xfer->xfer_type) {
		case TRANSFER_TYPE_CONTROL:
			return uroothub_ctrl_xfer(dev, xfer);
		case TRANSFER_TYPE_HUB_ATTACH_DONE:
			hub_privdata->hub_flags &= ~HUB_FLAG_ATTACHING;
			return ANANAS_ERROR_OK;
	}
	panic("unsupported transfer type");
}

static void
uhci_hub_pollthread(void* ptr)
{
	device_t dev = ptr;
	struct UHCI_HUB_PRIVDATA* hub_privdata = dev->privdata;
	struct UHCI_HCD_PRIVDATA* hcd_privdata = hub_privdata->hub_uhcidev->privdata;

	while (1) {
		for (int portno = 0; portno < hub_privdata->hub_numports; portno++) {
			int hub_ioport = hcd_privdata->uhci_io + UHCI_REG_PORTSC1 + portno * 2;
			int stat = inw(hub_ioport);

			if (stat & UHCI_PORTSC_CSCHANGE) {
				/*
				 * If we are currently attaching a device, don't continue; there can be only one
				 * device to be attached on the bus at any time.
				 */
				if (hub_privdata->hub_flags & HUB_FLAG_ATTACHING)
					break;
				device_printf(dev, "port %u changed state to %s", portno,
				 (stat & UHCI_PORTSC_CONNSTAT) ? "present" : "removed");
				if (stat & UHCI_PORTSC_CONNSTAT) {
					/*
					 * There is a new device here, we must reset it. XXX Note that we
					 * must never reset more than a single device; this needs work.
					 */
					outw(hub_ioport, UHCI_PORTSC_MASK(stat) | UHCI_PORTSC_RESET);
					delay(10); /* We must wait exactly 10ms ... */
					outw(hub_ioport, UHCI_PORTSC_MASK(inw(hub_ioport)) & ~UHCI_PORTSC_RESET);

					/* Enable the port */
					int tries = 10;
					for (; tries > 0; tries--) {
						delay(10);
						int x = inw(hub_ioport);
						if ((x & UHCI_PORTSC_CONNSTAT) == 0) {
							device_printf(dev, "device gone while attaching port %u, giving up", portno);
							break;
						}

						if (x & (UHCI_PORTSC_CSCHANGE | UHCI_PORTSC_PECHANGE)) {
							/* Status changed; try again until it's stable */
							outw(hub_ioport, (x & (UHCI_PORTSC_CSCHANGE | UHCI_PORTSC_PECHANGE)));
							continue;
						}

						if (x & UHCI_PORTSC_PORTEN)
							break;

						/* Enable the port */
						outw(hub_ioport, x | UHCI_PORTSC_PORTEN);
					}

					if (tries > 0) {
						/* Alright, the device seems stable - time to initialize it */
						hub_privdata->hub_flags |= HUB_FLAG_ATTACHING;
						usb_attach_device(hub_privdata->hub_uhcidev, dev);
					} else {
						device_printf(dev, "port %u reset timeout", portno);
					}
				} else {
					/* Device was removed; acknowledge the notification */
					outw(hub_ioport, stat & (UHCI_PORTSC_CSCHANGE | UHCI_PORTSC_PECHANGE));
				}
			}

			/* XXX This is way too often */
			reschedule();
		}
	}
}

static errorcode_t
uroothub_attach(device_t dev)
{
	struct UHCI_HUB_PRIVDATA* hub_privdata = dev->privdata;
	device_printf(dev, "%u port(s)", hub_privdata->hub_numports);

	/* Create a kernel thread to monitor status updates */
  thread_init(&hub_privdata->hub_pollthread, NULL);
  thread_set_args(&hub_privdata->hub_pollthread, "[uroothub]\0\0", PAGE_SIZE);
	md_thread_setkthread(&hub_privdata->hub_pollthread, uhci_hub_pollthread, hub_privdata->hub_dev);
	thread_resume(&hub_privdata->hub_pollthread);
	return ANANAS_ERROR_OK;
}

static struct DRIVER drv_uroothub = {
	.name             = "uroothub",
	.drv_attach       = uroothub_attach,
	.drv_roothub_xfer = uroothub_xfer
};

errorcode_t
uhci_hub_create(device_t uhci_dev, int numports)
{
	struct UHCI_HUB_PRIVDATA* hub_privdata = kmalloc(sizeof *hub_privdata);

	hub_privdata->hub_uhcidev = uhci_dev;
	hub_privdata->hub_numports = numports;
	hub_privdata->hub_flags = 0;

	/* Attach the root hub like a device; this makes it show up in the device tree */
	hub_privdata->hub_dev = device_alloc(uhci_dev, &drv_uroothub);
	hub_privdata->hub_dev->privdata = hub_privdata;
	return device_attach_single(hub_privdata->hub_dev);
}

/* vim:set ts=2 sw=2: */
