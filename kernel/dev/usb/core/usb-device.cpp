#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/device.h>
#include <ananas/list.h>
#include <ananas/lib.h>
#include <ananas/thread.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/mm.h>
#include <machine/param.h> /* for PAGE_SIZE XXX */
#include "config.h"
#include "usb-bus.h"
#include "usb-core.h"
#include "usb-device.h"
#include "usb-transfer.h"
#include "../device/usb-hub.h"

TRACE_SETUP;

namespace Ananas {
namespace USB {

USBDevice::USBDevice(Bus& bus, Hub* hub, int hub_port, int flags)
	: ud_bus(bus), ud_hub(hub), ud_port(hub_port), ud_flags(flags)
{
	//void* hcd_privdata = bus->bus_hcd->driver->drv_usb_hcd_initprivdata(flags);

	mutex_init(&ud_mutex, "usbdev");
	//usb_dev->usb_device = device_alloc(bus->bus_dev, NULL);
	//usb_dev->usb_hcd_privdata = hcd_privdata;
	//usb_dev->usb_device->d_resourceset.AddResource(Ananas::Resource(Ananas::Resource::RT_USB_Device, reinterpret_cast<Ananas::Resource::Base>(usb_dev), 0));

	LIST_INIT(&ud_transfers);
	LIST_INIT(&ud_pipes);
	memset(&ud_interface, 0, sizeof(ud_interface));
	memset(&ud_descr_device, 0, sizeof(ud_descr_device));
}

USBDevice::~USBDevice()
{
	delete ud_device; // XXX likely we need to do something else here...
}

/*
 * Attaches a single USB device (which hasn't got an address or anything yet)
 *
 * Should _only_ be called by the usb-bus thread!
 */
errorcode_t
USBDevice::Attach()
{
	/*
	 * First step is to reset the port - we do this here to prevent multiple ports from being
	 * reset.
	 *
	 * Note that usb_hub can be NULL if we're attaching the root hub itself.
	 */
	if (ud_hub != nullptr) {
		errorcode_t err = ud_hub->ResetPort(ud_port);
		ANANAS_ERROR_RETURN(err);
	}

	/*
	 * Obtain the first 8 bytes of the device descriptor; this tells us how how
	 * large the control endpoint requests can be.
	 */
	{
		size_t len = 8;
		errorcode_t err = PerformControlTransfer(USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_STANDARD, USB_REQUEST_MAKE(USB_DESCR_TYPE_DEVICE, 0), 0, &ud_descr_device, &len, false);
		ANANAS_ERROR_RETURN(err);

		TRACE(USB, INFO,
		 "got partial device descriptor: len=%u, type=%u, version=%u, class=%u, subclass=%u, protocol=%u, maxsize=%u",
			ud_descr_device.dev_length, ud_descr_device.dev_type, ud_descr_device.dev_version, ud_descr_device.dev_class,
			ud_descr_device.dev_subclass, ud_descr_device.dev_protocol, ud_descr_device.dev_maxsize0);

		// Store the maximum endpoint 0 packet size 
		ud_max_packet_sz0 = ud_descr_device.dev_maxsize0;
	}

	// Address phase
	{
		/* Construct a device address */
		int dev_address = ud_bus.AllocateAddress();
		if (dev_address <= 0) {
			kprintf("out of addresses, aborting attachment!\n");
			return ANANAS_ERROR(NO_RESOURCE);
		}

		/* Assign the device a logical address */
		errorcode_t err = PerformControlTransfer(USB_CONTROL_REQUEST_SET_ADDRESS, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_STANDARD, dev_address, 0, NULL, NULL, true);
		ANANAS_ERROR_RETURN(err);

		/*
		 * Address configured - we could attach more things in parallel from now on,
		 * but this only complicates things, so let's not go there...
		 */
		ud_address = dev_address;
		TRACE(USB, INFO, "logical address is %u", ud_address);
	}

	/* Now, obtain the entire device descriptor */
	{
		size_t len = sizeof(ud_descr_device);
		errorcode_t err = PerformControlTransfer(USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_STANDARD, USB_REQUEST_MAKE(USB_DESCR_TYPE_DEVICE, 0), 0, &ud_descr_device, &len, false);
		ANANAS_ERROR_RETURN(err);

		TRACE(USB, INFO,
		 "got full device descriptor: len=%u, type=%u, version=%u, class=%u, subclass=%u, protocol=%u, maxsize=%u, vendor=%u, product=%u numconfigs=%u",
			ud_descr_device.dev_length, ud_descr_device.dev_type, ud_descr_device.dev_version, ud_descr_device.dev_class,
			ud_descr_device.dev_subclass, ud_descr_device.dev_protocol, ud_descr_device.dev_maxsize0, ud_descr_device.dev_vendor,
			ud_descr_device.dev_product, ud_descr_device.dev_num_configs);
	}

	{
		/* Obtain the language ID of this device */
		struct USB_DESCR_STRING s;
		size_t len = 4  /* just the first language */ ;
		errorcode_t err = PerformControlTransfer(USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_STANDARD, USB_REQUEST_MAKE(USB_DESCR_TYPE_STRING, 0), 0, &s, &len, false);
		ANANAS_ERROR_RETURN(err);

		/* Retrieved string language code */
		uint16_t langid = s.u.str_langid[0];
		TRACE(USB, INFO, "got language code, first is %u", langid);

		/* Time to fetch strings; this must be done in two steps: length and content */
		len = 4 /* length only */ ;
		err = PerformControlTransfer(USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_STANDARD, USB_REQUEST_MAKE(USB_DESCR_TYPE_STRING, ud_descr_device.dev_productidx), langid, &s, &len, false);
		ANANAS_ERROR_RETURN(err);

		/* Retrieved string length */
		TRACE(USB, INFO, "got string length=%u", s.str_length);

		/* Fetch the entire string this time */
		char tmp[1024]; /* XXX */
		auto s_full = reinterpret_cast<struct USB_DESCR_STRING*>(tmp);
		len = s.str_length;
		KASSERT(len < sizeof(tmp), "very large string descriptor %u", s.str_length);
		err = PerformControlTransfer(USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_STANDARD, USB_REQUEST_MAKE(USB_DESCR_TYPE_STRING, ud_descr_device.dev_productidx), langid, s_full, &len, false);
		ANANAS_ERROR_RETURN(err);

		kprintf("product <");
		for (int i = 0; i < s_full->str_length / 2; i++) {
			kprintf("%c", s_full->u.str_string[i] & 0xff);
		}
		kprintf(">\n");
	}

	/*
	 * Grab the first few bytes of configuration descriptor. Note that we
	 * have no idea how long the configuration exactly is, so we must
	 * do this in two steps.
	 */
	{
		struct USB_DESCR_CONFIG c;
		size_t len = sizeof(c);
		errorcode_t err = PerformControlTransfer(USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_STANDARD, USB_REQUEST_MAKE(USB_DESCR_TYPE_CONFIG, 0), 0, &c, &len, false);
		ANANAS_ERROR_RETURN(err);

		/* Retrieved partial config descriptor */
		TRACE(USB, INFO,
		 "got partial config descriptor: len=%u, num_interfaces=%u, id=%u, stringidx=%u, attrs=%u, maxpower=%u, total=%u",
		 c.cfg_length, c.cfg_numinterfaces, c.cfg_identifier, c.cfg_stringidx, c.cfg_attrs, c.cfg_maxpower,
		 c.cfg_totallen);

		/* Fetch the full descriptor */
		char tmp[1024]; /* XXX */
		auto c_full = reinterpret_cast<struct USB_DESCR_CONFIG*>(tmp);
		len = c.cfg_totallen;
		KASSERT(len < sizeof(tmp), "very large configuration descriptor %u", c.cfg_totallen);
		err = PerformControlTransfer(USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_STANDARD, USB_REQUEST_MAKE(USB_DESCR_TYPE_CONFIG, 0), 0, c_full, &len, false);
		ANANAS_ERROR_RETURN(err);

		/* Retrieved full device descriptor */
		TRACE(USB, INFO, "got full config descriptor");

		/* Handle the configuration */
		err = ParseConfiguration(ud_interface, ud_num_interfaces, c_full, c.cfg_totallen);
		ANANAS_ERROR_RETURN(err);

		/* For now, we'll just activate the very first configuration */
		err = PerformControlTransfer(USB_CONTROL_REQUEST_SET_CONFIGURATION, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_STANDARD, c_full->cfg_identifier, 0, NULL, NULL, true);
		ANANAS_ERROR_RETURN(err);

		/* Configuration activated */
		TRACE(USB, INFO, "configuration activated");
		ud_cur_interface = 0;
	}

	/* Now, we'll have to hook up some driver... */
	Ananas::ResourceSet resourceSet;
	resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_USB_Device, reinterpret_cast<Ananas::Resource::Base>(this), 0));
	ud_device = Ananas::DeviceManager::AttachChild(ud_bus, resourceSet);
	KASSERT(ud_device != nullptr, "unable to find USB device to attach?");

	return ananas_success();
}

/* Called with bus lock held */
errorcode_t
USBDevice::Detach()
{
	ud_bus.AssertLocked();

	// Stop any pending pipes - this should cancel any pending transfers, which ensures Detach()
	// can run without any callbacks in between
	Lock();
	LIST_FOREACH(&ud_pipes, pipe, Pipe) {
		pipe->p_xfer.Cancel_Locked();
	}
	Unlock();

	// Ask the device to clean up after itself
	if (ud_device != nullptr) {
		errorcode_t err = DeviceManager::Detach(*ud_device);
		ANANAS_ERROR_RETURN(err);
		ud_device = nullptr;
	}

	Lock();
	KASSERT(LIST_EMPTY(&ud_pipes), "device detach with active pipes");
	KASSERT(LIST_EMPTY(&ud_transfers), "device detach with active transfers");

	/* Remove the device from the bus - note that we hold the bus lock */
	LIST_REMOVE(&ud_bus.bus_devices, this);

	// All set; the device is eligable for destruction now
	Unlock();
	return ananas_success();
}

namespace {

void
CallbackWrapper(Transfer& xfer)
{
	auto pipe = static_cast<Pipe*>(xfer.t_callback_data);
	pipe->p_callback.OnPipeCallback(*pipe);
}

} // unnamed namespace

errorcode_t
USBDevice::AllocatePipe(int num, int type, int dir, size_t maxlen, IPipeCallback& callback, Pipe*& pipe)
{
	KASSERT(dir == EP_DIR_IN || dir == EP_DIR_OUT, "invalid direction %u", dir);
	KASSERT(type == TRANSFER_TYPE_CONTROL || type == TRANSFER_TYPE_INTERRUPT || type == TRANSFER_TYPE_BULK || type == TRANSFER_TYPE_ISOCHRONOUS, "invalid type %u", type);

	Interface& uif = ud_interface[ud_cur_interface];
	if (num < 0 || num >= uif.if_num_endpoints)
		return ANANAS_ERROR(BAD_RANGE);

	Endpoint& ep = uif.if_endpoint[num];
	if (ep.ep_type != type || ep.ep_dir != dir)
		return ANANAS_ERROR(BAD_TYPE);

	if (maxlen == 0)
		maxlen = ep.ep_maxpacketsize;

	Transfer* xfer = AllocateTransfer(*this, ep.ep_type, ((ep.ep_dir == EP_DIR_IN) ? TRANSFER_FLAG_READ : TRANSFER_FLAG_WRITE) | TRANSFER_FLAG_DATA, ep.ep_address, maxlen);
	auto p = new Pipe(*this, ep, *xfer, callback);

	xfer->t_length = ep.ep_maxpacketsize;
	xfer->t_callback = CallbackWrapper;
	xfer->t_callback_data = p;

	/* Hook up the pipe to the device */
	Lock();
	LIST_APPEND(&ud_pipes, p);
	Unlock();
	pipe = p;
	return ananas_success();
}

void
USBDevice::FreePipe(Pipe& pipe)
{
	Lock();
	FreePipe_Locked(pipe);
	Unlock();
}

void
USBDevice::FreePipe_Locked(Pipe& pipe)
{
	/* Free the transfer (this cancels it as well) */
	FreeTransfer_Locked(pipe.p_xfer);

	/* Unregister the pipe - this is where we need the lock for */
	LIST_REMOVE(&ud_pipes, &pipe);
	delete &pipe;
}

errorcode_t
Pipe::Start()
{
	return p_xfer.Schedule();
}

errorcode_t
Pipe::Stop()
{
	return p_xfer.Cancel();
}

errorcode_t
USBDevice::PerformControlTransfer(int req, int recipient, int type, int value, int index, void* buf, size_t* len, bool write)
{
	int flags = 0;
	if (buf != nullptr)
		flags |= TRANSFER_FLAG_DATA;
	if (write)
		flags |= TRANSFER_FLAG_WRITE;
	else
		flags |= TRANSFER_FLAG_READ;
	auto xfer = AllocateTransfer(*this, TRANSFER_TYPE_CONTROL, flags, 0, (len != NULL) ? *len : 0);
	xfer->t_control_req.req_type = TO_REG32((write ? 0 : USB_CONTROL_REQ_DEV2HOST) | USB_CONTROL_REQ_RECIPIENT(recipient) | USB_CONTROL_REQ_TYPE(type));
	xfer->t_control_req.req_request = TO_REG32(req);
	xfer->t_control_req.req_value = TO_REG32(value);
	xfer->t_control_req.req_index = index;
	xfer->t_control_req.req_length = (len != NULL) ? *len : 0;
	xfer->t_length = (len != NULL) ? *len : 0;

	/* Now schedule the transfer and until it's completed XXX Timeout */
	xfer->Schedule();
	sem_wait_and_drain(&xfer->t_semaphore);
	if (xfer->t_flags & TRANSFER_FLAG_ERROR) {
		FreeTransfer(*xfer);
		return ANANAS_ERROR(IO);
	}

	if (buf != nullptr && len != nullptr) {
		/* XXX Ensure we don't return more than the user wants to know */
		if (*len > xfer->t_result_length)
			*len = xfer->t_result_length;
		memcpy(buf, xfer->t_data, *len);
	}

	FreeTransfer(*xfer);
	return ananas_success();
}

} // namespace USB
} // namespace Ananas

/* vim:set ts=2 sw=2: */
