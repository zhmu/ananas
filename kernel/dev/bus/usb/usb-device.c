#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/bus/usb/config.h>
#include <ananas/bus/usb/core.h>
#include <ananas/dqueue.h>
#include <ananas/lib.h>
#include <ananas/thread.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/mm.h>

TRACE_SETUP;

static struct USB_TRANSFER*
usb_create_get_descriptor_xfer(struct USB_DEVICE* usb_dev, int type, int v, int index, int len)
{
	struct USB_TRANSFER* xfer = usb_alloc_transfer(usb_dev, TRANSFER_TYPE_CONTROL, TRANSFER_FLAG_READ | TRANSFER_FLAG_DATA, 0);
	xfer->xfer_control_req.req_type = TO_REG32(USB_CONTROL_REQ_DEV2HOST | USB_CONTROL_RECIPIENT_DEVICE | USB_CONTROL_REQ_TYPE(USB_CONTROL_TYPE_STANDARD));
	xfer->xfer_control_req.req_request = TO_REG32(USB_CONTROL_REQUEST_GET_DESC);
	xfer->xfer_control_req.req_value = TO_REG32(type << 8 | v);
	xfer->xfer_control_req.req_index = index;
	xfer->xfer_control_req.req_length = len;
	xfer->xfer_length = len;
	return xfer;
}

static void	
usb_attach_driver(struct USB_DEVICE* usb_dev)
{
	KASSERT(usb_dev->usb_cur_interface >= 0, "attach on a device without an interface?");

	device_attach_bus(usb_dev->usb_device);
}

static void
usb_hub_attach_done(device_t usb_hub)
{
	struct USB_TRANSFER xfer_hub_ack;
	xfer_hub_ack.xfer_type = TRANSFER_TYPE_HUB_ATTACH_DONE;
	usb_hub->driver->drv_roothub_xfer(usb_hub, &xfer_hub_ack);
}

static void
usb_attach_callback(struct USB_TRANSFER* usb_xfer)
{
	struct USB_DEVICE* usb_dev = usb_xfer->xfer_device;	
	struct USB_TRANSFER* next_xfer = NULL;
	struct DEVICE* dev = usb_dev->usb_device;

/*
	kprintf("xfer real / length = %u / %u\n",
	 usb_xfer->xfer_length, usb_xfer->xfer_result_length);
 */

	/* If anything went wrong, abort the attach */
	if (usb_xfer->xfer_flags & TRANSFER_FLAG_ERROR) {
		device_printf(dev, "usb communication failure, aborting attach");
		usb_hub_attach_done(usb_dev->usb_hub);

		/* XXX We should clean up the device */
		return;
	}

	switch(usb_dev->usb_attachstep++ /* <== mind the ++ */) {
		case 0: { /* Retrieved partial device descriptor */
			struct USB_DESCR_DEVICE* d = (void*)usb_xfer->xfer_data;
			TRACE_DEV(USB, INFO, dev,
			 "got partial device descriptor: len=%u, type=%u, version=%u, class=%u, subclass=%u, protocol=%u, maxsize=%u",
				d->dev_length, d->dev_type, d->dev_version, d->dev_class,
				d->dev_subclass, d->dev_protocol, d->dev_maxsize0);

			/* Store the maximum endpoint 0 packet size */
			usb_dev->usb_max_packet_sz0 = d->dev_maxsize0;

			/* Construct a device address */
			int dev_address = usb_get_next_address(usb_dev);

			/* Assign the device a logical address */
			next_xfer = usb_alloc_transfer(usb_dev, TRANSFER_TYPE_CONTROL, TRANSFER_FLAG_WRITE, 0);
			next_xfer->xfer_control_req.req_type = TO_REG32(USB_CONTROL_RECIPIENT_DEVICE | USB_CONTROL_REQ_TYPE(USB_CONTROL_TYPE_STANDARD));
			next_xfer->xfer_control_req.req_request = TO_REG32(USB_CONTROL_REQUEST_SET_ADDRESS);
			next_xfer->xfer_control_req.req_value = dev_address;
			next_xfer->xfer_control_req.req_index = 0;
			next_xfer->xfer_control_req.req_length = 0;

			/* And update the new address */
			usb_dev->usb_address = dev_address;
			break;
		}
		case 1: { /* Address configured */
			TRACE_DEV(USB, INFO, usb_dev->usb_device, "logical address is %u", usb_dev->usb_address);

			/* We can now let the hub know that it can continue resetting new devices */
			usb_hub_attach_done(usb_dev->usb_hub);

			/* Now, obtain the entire device descriptor */
			next_xfer = usb_create_get_descriptor_xfer(usb_dev, USB_DESCR_TYPE_DEVICE, 0, 0, sizeof(struct USB_DESCR_DEVICE));
			break;
		}
		case 2: { /* Retrieved full device descriptor */
			struct USB_DESCR_DEVICE* d = (void*)usb_xfer->xfer_data;

			TRACE_DEV(USB, INFO, dev,
			 "got full device descriptor: len=%u, type=%u, version=%u, class=%u, subclass=%u, protocol=%u, maxsize=%u, vendor=%u, product=%u numconfigs=%u",
				d->dev_length, d->dev_type, d->dev_version, d->dev_class,
				d->dev_subclass, d->dev_protocol, d->dev_maxsize0, d->dev_vendor,
				d->dev_product, d->dev_num_configs);
			memcpy(&usb_dev->usb_descr_device, usb_xfer->xfer_data, sizeof(usb_dev->usb_descr_device));

			/* XXX We only grab the product name */
			usb_dev->usb_cur_string = d->dev_productidx;

			/* Obtain the language ID of this device */
			next_xfer = usb_create_get_descriptor_xfer(usb_dev, USB_DESCR_TYPE_STRING, 0, 0, 4 /* just the first language */);
			break;
		}
		case 3: { /* Retrieved string language code */
			struct USB_DESCR_STRING* s = (void*)usb_xfer->xfer_data;
			usb_dev->usb_langid = s->u.str_langid[0];
			TRACE_DEV(USB, INFO, dev, "got language code, first is %u", usb_dev->usb_langid);

			/* Time to fetch strings; this must be done in two steps: length and content */
			next_xfer = usb_create_get_descriptor_xfer(usb_dev, USB_DESCR_TYPE_STRING, usb_dev->usb_cur_string, usb_dev->usb_langid, 4 /* length only */);
			break;
		}
		case 4: { /* Retrieved string length */
			struct USB_DESCR_STRING* s = (void*)usb_xfer->xfer_data;
			TRACE_DEV(USB, INFO, dev, "got string length=%u", s->str_length);

			/* Fetch the entire string this time */
			next_xfer = usb_create_get_descriptor_xfer(usb_dev, USB_DESCR_TYPE_STRING, usb_dev->usb_cur_string, usb_dev->usb_langid, s->str_length);
			break;
		}
		case 5: { /* Retrieve string data */
			struct USB_DESCR_STRING* s = (void*)usb_xfer->xfer_data;

			kprintf("%s%u: product <", dev->name, dev->unit);
			for (int i = 0; i < s->str_length / 2; i++) {
				kprintf("%c", s->u.str_string[i] & 0xff);
			}
			kprintf(">\n");

			/*
			 * Grab the first few bytes of configuration descriptor. Note that we
			 * have no idea how long the configuration exactly is, so we must
			 * do this in two steps.
			 */
			next_xfer = usb_create_get_descriptor_xfer(usb_dev, USB_DESCR_TYPE_CONFIG, 0, 0, sizeof(struct USB_DESCR_CONFIG));
			break;
		}
		case 6: { /* Retrieved partial config descriptor */
			struct USB_DESCR_CONFIG* c = (void*)usb_xfer->xfer_data;

			TRACE_DEV(USB, INFO, dev,
			 "got partial config descriptor: len=%u, num_interfaces=%u, id=%u, stringidx=%u, attrs=%u, maxpower=%u, total=%u",
			 c->cfg_length, c->cfg_numinterfaces, c->cfg_identifier, c->cfg_stringidx, c->cfg_attrs, c->cfg_maxpower,
			 c->cfg_totallen);

			/* Fetch the full descriptor */
			next_xfer = usb_create_get_descriptor_xfer(usb_dev, USB_DESCR_TYPE_CONFIG, 0, 0, c->cfg_totallen);
			break;
		}
		case 7: { /* Retrieved full device descriptor */
			TRACE_DEV(USB, INFO, dev, "got full config descriptor");

			/* Handle the configuration */
			struct USB_DESCR_CONFIG* c = (void*)usb_xfer->xfer_data;
			errorcode_t err = usb_parse_configuration(usb_dev, usb_xfer->xfer_data, c->cfg_totallen);
			KASSERT(err == ANANAS_ERROR_OK, "cannot yet deal with failures");

			/* For now, we'll just activate the very first configuration */
			next_xfer = usb_alloc_transfer(usb_dev, TRANSFER_TYPE_CONTROL, TRANSFER_FLAG_WRITE, 0);
			next_xfer->xfer_control_req.req_type = TO_REG32(USB_CONTROL_RECIPIENT_DEVICE | USB_CONTROL_REQ_TYPE(USB_CONTROL_TYPE_STANDARD));
			next_xfer->xfer_control_req.req_request = TO_REG32(USB_CONTROL_REQUEST_SET_CONFIGURATION);
			next_xfer->xfer_control_req.req_value = c->cfg_identifier;
			next_xfer->xfer_control_req.req_index = 0;
			next_xfer->xfer_control_req.req_length = 0;
			next_xfer->xfer_length = 0;
			break;
		}
		case 8: { /* Configuration activated */
			TRACE_DEV(USB, INFO, dev, "configuration activated");
			usb_dev->usb_cur_interface = 0;

			/* Now, we'll have to hook up some driver... */
			usb_attach_driver(usb_dev);
			break;
		}
	}

	if (next_xfer != NULL) {
		next_xfer->xfer_callback = usb_attach_callback;
		usb_schedule_transfer(next_xfer);
	}
}

void
usb_attach_device(device_t parent, device_t hub, void* hcd_privdata)
{
	struct USB_DEVICE* usb_dev = usb_alloc_device(parent, hub, hcd_privdata);

	/* Now at step 0 */
	usb_dev->usb_attachstep = 0;

	/*
	 * First of all, obtain the first 8 bytes of the device descriptor; this tells us how
	 * how large the control endpoint requests can be.
	 */
	struct USB_TRANSFER* xfer = usb_create_get_descriptor_xfer(usb_dev, USB_DESCR_TYPE_DEVICE, 0, 0, 8);
	xfer->xfer_callback = usb_attach_callback;
	usb_schedule_transfer(xfer);
}

/* vim:set ts=2 sw=2: */
