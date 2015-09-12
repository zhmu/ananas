#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/bus/usb/config.h>
#include <ananas/bus/usb/core.h>
#include <ananas/bus/usb/transfer.h>
#include <ananas/dqueue.h>
#include <ananas/lib.h>
#include <ananas/thread.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/mm.h>
#include <machine/param.h> /* for PAGE_SIZE XXX */

TRACE_SETUP;

static thread_t usb_devicethread;
static semaphore_t usb_device_semaphore;
static struct USB_DEVICE_QUEUE usb_device_pendingqueue;
static spinlock_t usb_device_spl_pendingqueue = SPINLOCK_DEFAULT_INIT;

static void	
usb_attach_driver(struct USB_DEVICE* usb_dev)
{
	KASSERT(usb_dev->usb_cur_interface >= 0, "attach on a device without an interface?");

	device_attach_bus(usb_dev->usb_device);
}

static void
usb_hub_attach_done(device_t usb_hub)
{
	/* XXX This no longer works; need to clean up the hub <-> hcd <-> device mess */
#if 0
	struct USB_TRANSFER xfer_hub_ack;
	xfer_hub_ack.xfer_type = TRANSFER_TYPE_HUB_ATTACH_DONE;
	usb_hub->parent->driver->drv_usb_xfer(usb_hub, &xfer_hub_ack);
#endif
}

static errorcode_t
usb_attach_device_one(struct USB_DEVICE* usb_dev)
{
	struct DEVICE* dev = usb_dev->usb_device;
	char tmp[1024]; /* XXX */
	size_t len;
	errorcode_t err;

	/*
	 * First of all, obtain the first 8 bytes of the device descriptor; this
	 * tells us how how large the control endpoint requests can be.
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
	int dev_address = usb_get_next_address(usb_dev);

	/* Assign the device a logical address */
	err = usb_control_xfer(usb_dev, USB_CONTROL_REQUEST_SET_ADDRESS, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_STANDARD, dev_address, 0, NULL, NULL, 1);
	ANANAS_ERROR_RETURN(err);

	/* Address configured */
	usb_dev->usb_address = dev_address;
	TRACE_DEV(USB, INFO, usb_dev->usb_device, "logical address is %u", usb_dev->usb_address);

	/* We can now let the hub know that it can continue resetting new devices */
	usb_hub_attach_done(usb_dev->usb_hub);

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

	/* Now, we'll have to hook up some driver... */
	usb_attach_driver(usb_dev);
	return ANANAS_ERROR_OK;
}

void
usb_device_thread(void* unused)
{
	while(1) {
		/* Wait until we have to wake up... */
		sem_wait(&usb_device_semaphore);

		/* Keep attaching devices */
		while(1) {
			spinlock_lock(&usb_device_spl_pendingqueue);
			if (DQUEUE_EMPTY(&usb_device_pendingqueue)) {
				spinlock_unlock(&usb_device_spl_pendingqueue);
				break;
			}
			struct USB_DEVICE* usb_dev = DQUEUE_HEAD(&usb_device_pendingqueue);
			DQUEUE_POP_HEAD(&usb_device_pendingqueue);
			spinlock_unlock(&usb_device_spl_pendingqueue);

			/*
			 * We need to attach this device; the first step is to enable port power. We do this
			 * here not to clutter the attach code with enabling/disabling power; it makes the
			 * former much more readible.
			 *
			 * Note that this will block until completed, which is intentional as it
			 * ensures we will never attach more than one device in the system at any
			 * given time.
			 */

			errorcode_t err = usb_attach_device_one(usb_dev);
			KASSERT(err == ANANAS_ERROR_OK, "cannot yet deal with failures");
		}
	}
}

void
usb_attach_device(device_t parent, device_t hub, void* hcd_privdata)
{
	struct USB_DEVICE* usb_dev = usb_alloc_device(parent, hub, hcd_privdata);

	/* Add the device to our queue */
	spinlock_lock(&usb_device_spl_pendingqueue);
	DQUEUE_ADD_TAIL(&usb_device_pendingqueue, usb_dev);
	spinlock_unlock(&usb_device_spl_pendingqueue);

	/* Wake up our thread */
	sem_signal(&usb_device_semaphore);
}

void
usb_attach_init()
{
	sem_init(&usb_device_semaphore, 0);
	DQUEUE_INIT(&usb_device_pendingqueue);

	/*
	 * Create a kernel thread to handle USB device attachments. We use a thread for this
	 * since we can only attach one at the same time.
	 */
	kthread_init(&usb_devicethread, &usb_device_thread, NULL);
	thread_set_args(&usb_devicethread, "[usbdevice]\0\0", PAGE_SIZE);
	thread_resume(&usb_devicethread);
}

/* vim:set ts=2 sw=2: */
