#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/bus/usb/config.h>
#include <ananas/bus/usb/core.h>
#include <ananas/bus/usb/transfer.h>
#include <ananas/device.h>
#include <ananas/LIST.h>
#include <ananas/lib.h>
#include <ananas/thread.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/mm.h>
#include <machine/param.h> /* for PAGE_SIZE XXX */
#include "usb-bus.h"
#include "usb-hub.h"
#include "usb-device.h"
#include "usb-transfer.h"

TRACE_SETUP;

extern struct DRIVER drv_usbgeneric;
extern struct DEVICE_PROBE probe_queue; /* XXX gross */

struct USB_DEVICE*
usb_alloc_device(struct USB_BUS* bus, struct USB_HUB* hub, int hub_port, int flags)
{
	void* hcd_privdata = bus->bus_hcd->driver->drv_usb_hcd_initprivdata(flags);

	struct USB_DEVICE* usb_dev = kmalloc(sizeof *usb_dev);
	memset(usb_dev, 0, sizeof *usb_dev);
	mutex_init(&usb_dev->usb_mutex, "usbdev");
	usb_dev->usb_bus = bus;
	usb_dev->usb_hub = hub;
	usb_dev->usb_port = hub_port;
	usb_dev->usb_device = device_alloc(bus->bus_dev, NULL);
	usb_dev->usb_hcd_privdata = hcd_privdata;
	usb_dev->usb_address = 0;
	usb_dev->usb_max_packet_sz0 = USB_DEVICE_DEFAULT_MAX_PACKET_SZ0;
	usb_dev->usb_num_interfaces = 0;
	usb_dev->usb_cur_interface = -1;
	device_add_resource(usb_dev->usb_device, RESTYPE_USB_DEVICE, (resource_base_t)usb_dev, 0);

	LIST_INIT(&usb_dev->usb_pipes);
	return usb_dev;
}

static void
usbdev_free(struct USB_DEVICE* usb_dev)
{
	device_free(usb_dev->usb_device);
	kfree(usb_dev);
}

/*
 * Attaches a single USB device (which hasn't got an address or anything yet)
 *
 * Should _only_ be called by the usb-bus thread!
 */
errorcode_t
usbdev_attach(struct USB_DEVICE* usb_dev)
{
	struct DEVICE* dev = usb_dev->usb_device;
	char tmp[1024]; /* XXX */
	size_t len;
	errorcode_t err;

	/*
	 * First step is to reset the port - we do this here to prevent multiple ports from being
	 * reset.
	 *
	 * Note that usb_hub can be NULL if we're attaching the root hub itself.
	 */
	if (usb_dev->usb_hub != NULL) {
		err = ushub_reset_port(usb_dev->usb_hub, usb_dev->usb_port);
		ANANAS_ERROR_RETURN(err);
	}

	/*
	 * Obtain the first 8 bytes of the device descriptor; this tells us how how
	 * large the control endpoint requests can be.
	 */
	struct USB_DESCR_DEVICE* d = &usb_dev->usb_descr_device;
	len = 8;
	err = usb_control_xfer(usb_dev, USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_STANDARD, USB_REQUEST_MAKE(USB_DESCR_TYPE_DEVICE, 0), 0, d, &len, 0);
	ANANAS_ERROR_RETURN(err);

	TRACE_DEV(USB, INFO, dev,
	 "got partial device descriptor: len=%u, type=%u, version=%u, class=%u, subclass=%u, protocol=%u, maxsize=%u",
		d->dev_length, d->dev_type, d->dev_version, d->dev_class,
		d->dev_subclass, d->dev_protocol, d->dev_maxsize0);

	/* Store the maximum endpoint 0 packet size */
	usb_dev->usb_max_packet_sz0 = d->dev_maxsize0;

	/* Construct a device address */
	int dev_address = usbbus_alloc_address(usb_dev->usb_bus);
	if (dev_address <= 0) {
		device_printf(dev, "out of addresses on bus %s, aborting attachment!", usb_dev->usb_bus->bus_dev->name);
		return ANANAS_ERROR(NO_RESOURCE);
	}

	/* Assign the device a logical address */
	err = usb_control_xfer(usb_dev, USB_CONTROL_REQUEST_SET_ADDRESS, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_STANDARD, dev_address, 0, NULL, NULL, 1);
	ANANAS_ERROR_RETURN(err);

	/*
	 * Address configured - we could attach more things in parallel from now on,
	 * but this only complicates things to no benefit...
	 */
	usb_dev->usb_address = dev_address;
	TRACE_DEV(USB, INFO, usb_dev->usb_device, "logical address is %u", usb_dev->usb_address);

	/* Now, obtain the entire device descriptor */
	len = sizeof(usb_dev->usb_descr_device);
	err = usb_control_xfer(usb_dev, USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_STANDARD, USB_REQUEST_MAKE(USB_DESCR_TYPE_DEVICE, 0), 0, d, &len, 0);
	ANANAS_ERROR_RETURN(err);

	TRACE_DEV(USB, INFO, dev,
	 "got full device descriptor: len=%u, type=%u, version=%u, class=%u, subclass=%u, protocol=%u, maxsize=%u, vendor=%u, product=%u numconfigs=%u",
		d->dev_length, d->dev_type, d->dev_version, d->dev_class,
		d->dev_subclass, d->dev_protocol, d->dev_maxsize0, d->dev_vendor,
		d->dev_product, d->dev_num_configs);

	/* Obtain the language ID of this device */
	struct USB_DESCR_STRING s;
	len = 4  /* just the first language */ ;
	err = usb_control_xfer(usb_dev, USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_STANDARD, USB_REQUEST_MAKE(USB_DESCR_TYPE_STRING, 0), 0, &s, &len, 0);
	ANANAS_ERROR_RETURN(err);

	/* Retrieved string language code */
	uint16_t langid = s.u.str_langid[0];
	TRACE_DEV(USB, INFO, dev, "got language code, first is %u", langid);

	/* Time to fetch strings; this must be done in two steps: length and content */
	len = 4 /* length only */ ;
	err = usb_control_xfer(usb_dev, USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_STANDARD, USB_REQUEST_MAKE(USB_DESCR_TYPE_STRING, d->dev_productidx), langid, &s, &len, 0);
	ANANAS_ERROR_RETURN(err);

	/* Retrieved string length */
	TRACE_DEV(USB, INFO, dev, "got string length=%u", s.str_length);

	/* Fetch the entire string this time */
	struct USB_DESCR_STRING* s_full = (void*)tmp;
	len = s.str_length;
	KASSERT(len < sizeof(tmp), "very large string descriptor %u", s.str_length);
	err = usb_control_xfer(usb_dev, USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_STANDARD, USB_REQUEST_MAKE(USB_DESCR_TYPE_STRING, d->dev_productidx), langid, s_full, &len, 0);
	ANANAS_ERROR_RETURN(err);

	kprintf("%s%u: product <", dev->name, dev->unit);
	for (int i = 0; i < s_full->str_length / 2; i++) {
		kprintf("%c", s_full->u.str_string[i] & 0xff);
	}
	kprintf(">\n");

	/*
	 * Grab the first few bytes of configuration descriptor. Note that we
	 * have no idea how long the configuration exactly is, so we must
	 * do this in two steps.
	 */
	struct USB_DESCR_CONFIG c;
	len = sizeof(c);
	err = usb_control_xfer(usb_dev, USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_STANDARD, USB_REQUEST_MAKE(USB_DESCR_TYPE_CONFIG, 0), 0, &c, &len, 0);
	ANANAS_ERROR_RETURN(err);

	/* Retrieved partial config descriptor */
	TRACE_DEV(USB, INFO, dev,
	 "got partial config descriptor: len=%u, num_interfaces=%u, id=%u, stringidx=%u, attrs=%u, maxpower=%u, total=%u",
	 c.cfg_length, c.cfg_numinterfaces, c.cfg_identifier, c.cfg_stringidx, c.cfg_attrs, c.cfg_maxpower,
	 c.cfg_totallen);

	/* Fetch the full descriptor */
	struct USB_DESCR_CONFIG* c_full = (void*)tmp;
	len = c.cfg_totallen;
	KASSERT(len < sizeof(tmp), "very large configuration descriptor %u", c.cfg_totallen);
	err = usb_control_xfer(usb_dev, USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_STANDARD, USB_REQUEST_MAKE(USB_DESCR_TYPE_CONFIG, 0), 0, c_full, &len, 0);
	ANANAS_ERROR_RETURN(err);

	/* Retrieved full device descriptor */
	TRACE_DEV(USB, INFO, dev, "got full config descriptor");

	/* Handle the configuration */
	err = usb_parse_configuration(usb_dev, c_full, c.cfg_totallen);
	ANANAS_ERROR_RETURN(err);

	/* For now, we'll just activate the very first configuration */
	err = usb_control_xfer(usb_dev, USB_CONTROL_REQUEST_SET_CONFIGURATION, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_STANDARD, c_full->cfg_identifier, 0, NULL, NULL, 1);
	ANANAS_ERROR_RETURN(err);

	/* Configuration activated */
	TRACE_DEV(USB, INFO, dev, "configuration activated");
	usb_dev->usb_cur_interface = 0;

	/* Now, we'll have to hook up some driver... XXX this is duplicated from device.c/pcibus.c */
	int device_attached = 0;
	LIST_FOREACH(&probe_queue, p, struct PROBE) {
		/* See if the device lives on our bus */
		int exists = 0;
		for (const char** curbus = p->bus; *curbus != NULL; curbus++) {
			/*
			 * Note that we need to check the _parent_ as that is the bus; 'dev' will
			 * be turned into a fully-flegded device once we find a driver which
			 * likes it.
			 */
			if (strcmp(*curbus, dev->parent->name) == 0) {
				exists = 1;
				break;
			}
		}
		if (!exists)
			continue;

		/* This device may work - give it a chance to attach */
		dev->driver = p->driver;
		strcpy(dev->name, dev->driver->name);
		dev->unit = dev->driver->current_unit++;
		errorcode_t err = device_attach_single(dev);
		if (err == ANANAS_ERROR_NONE) {
			/* This worked; use the next unit for the new device */
			device_attached++;
			break;
		} else {
			/* No luck, revert the unit number */
			dev->driver->current_unit--;
		}
	}

	if (!device_attached) {
		/* Nothing found; revert to the generic USB device here */
		dev->driver = &drv_usbgeneric;
		strcpy(dev->name, dev->driver->name);
		dev->unit = dev->driver->current_unit++;
		errorcode_t err = device_attach_single(dev);
		KASSERT(err == ANANAS_ERROR_NONE, "usb generic device failed %d", err);
	}
	return ANANAS_ERROR_NONE;
}

/* Called with bus lock held */
errorcode_t
usbdev_detach(struct USB_DEVICE* usb_dev)
{
	kprintf("usbdev_detach: removing '%s'\n", usb_dev->usb_device->name);

	mutex_assert(&usb_dev->usb_bus->bus_mutex, MTX_LOCKED);
	mutex_lock(&usb_dev->usb_mutex);

	/* Remove any pipes the device has; this should get rid of all transfers  */
	while(!LIST_EMPTY(&usb_dev->usb_pipes)) {
		struct USB_PIPE* p = LIST_HEAD(&usb_dev->usb_pipes);
		usbpipe_free_locked(p);
	}

	/* Get rid of all pending transfers; a control transfer may sneak in between */
	while (!LIST_EMPTY(&usb_dev->usb_transfers)) {
		struct USB_TRANSFER* xfer = LIST_HEAD(&usb_dev->usb_transfers);
		/* Freeing the transfer will cancel them as needed */
		usbtransfer_free_locked(xfer);
	}

	/* Give the driver a chance to clean up */
	device_detach(usb_dev->usb_device);

	/* Remove the device from the bus - note that we hold the bus lock */
	struct USB_BUS* bus = usb_dev->usb_bus;
	LIST_REMOVE(&bus->bus_devices, usb_dev);

	/* Finally, get rid of the device itself */
	mutex_unlock(&usb_dev->usb_mutex);
	usbdev_free(usb_dev);

	return ANANAS_ERROR_NONE;
}

/* vim:set ts=2 sw=2: */
