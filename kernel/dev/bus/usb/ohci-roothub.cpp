/*
 * OHCI root hub
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
#include <ananas/time.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <machine/param.h> /* for PAGE_SIZE */
#include "ohci-reg.h"
#include "ohci-hcd.h"
#include "ohci-roothub.h"
#include "usb-bus.h"
#include "usb-device.h"
#include "usb-transfer.h"

TRACE_SETUP;

#if 0
# define DPRINTF device_printf
#else
# define DPRINTF(...)
#endif

static const struct USB_DESCR_DEVICE ohci_rh_device = {
	.dev_length = sizeof(struct USB_DESCR_DEVICE),
	.dev_type = 0,
	.dev_version = 0x101,
	.dev_class = USB_DESCR_CLASS_HUB,
	.dev_subclass = 0,
	.dev_protocol = 0,
	.dev_maxsize0 = 8,
	.dev_vendor = 0,
	.dev_product = 0,
	.dev_release = 0,
	.dev_manufactureridx = 2,
	.dev_productidx = 1,
	.dev_serialidx = 0,
	.dev_num_configs = 1,
};

static const struct ohci_rh_string {
	uint8_t s_len, s_type;
	uint16_t s_string[13];
} ohci_rh_strings[] = {
	/* supported languages */
	{
		.s_len = 4,
		.s_type = USB_DESCR_TYPE_STRING,
		.s_string = {
			1033
		}
	},
	/* Product ID */
	{
		.s_len = 28,
		.s_type = USB_DESCR_TYPE_STRING,
		.s_string = {
			'O', 'H', 'C', 'I', ' ',
			'r', 'o', 'o', 't', ' ',
			'h', 'u', 'b'
		}
	},
	/* Vendor ID */
	{
		.s_len = 14,
		.s_type = USB_DESCR_TYPE_STRING,
		.s_string = {
			'A', 'n', 'a', 'n', 'a', 's'
		}
	},
};

static struct {
	struct USB_DESCR_CONFIG d_config;
	struct USB_DESCR_INTERFACE d_interface;
	struct USB_DESCR_ENDPOINT d_endpoint;
} __attribute__((packed)) const ohci_rh_config = {
	/* Configuration */
	{
		.cfg_length = sizeof(struct USB_DESCR_CONFIG),
		.cfg_type = USB_DESCR_TYPE_CONFIG, 
		.cfg_totallen = sizeof(ohci_rh_config),
		.cfg_numinterfaces = 1,
		.cfg_identifier = 0,
		.cfg_stringidx = 0,
		.cfg_attrs = 0x40, /* self-powered */
		.cfg_maxpower = 0,
	},
	/* Interface */
	{
		.if_length = sizeof(struct USB_DESCR_INTERFACE),
		.if_type = USB_DESCR_TYPE_INTERFACE,
		.if_number = 1,
		.if_altsetting = 0,
		.if_numendpoints = 1,
		.if_class = USB_IF_CLASS_HUB,
		.if_subclass = 0,
		.if_protocol = 1,
		.if_interfaceidx = 0,
	},
	/* Endpoint */
	{
		.ep_length = sizeof(struct USB_DESCR_ENDPOINT),
		.ep_type = USB_DESCR_TYPE_ENDPOINT,
		.ep_addr = USB_EP_ADDR_IN | USB_EP_ADDR(1),
		.ep_attr = USB_PE_ATTR_TYPE_INTERRUPT,
		.ep_maxpacketsz = 8,
		.ep_interval = 255,
	}
};

static errorcode_t
oroothub_ctrl_xfer(device_t dev, struct USB_TRANSFER* xfer)
{
	struct USB_CONTROL_REQUEST* req = &xfer->xfer_control_req;
	auto p = static_cast<struct OHCI_PRIVDATA*>(dev->privdata);
	errorcode_t err = ANANAS_ERROR(BAD_OPERATION);

#define MIN(a, b) ((a) < (b) ? (a) : (b))

	switch(USB_REQUEST_MAKE(req->req_type, req->req_request)) {
		case USB_REQUEST_STANDARD_GET_DESCRIPTOR:
			switch(req->req_value >> 8) {
				case USB_DESCR_TYPE_DEVICE: {
					int amount = MIN(ohci_rh_device.dev_length, req->req_length);
					memcpy(xfer->xfer_data, &ohci_rh_device, amount);
					xfer->xfer_result_length = amount;
					err = ananas_success();
					break;
				}
				case USB_DESCR_TYPE_STRING: {
					int string_id = req->req_value & 0xff;
					if (string_id >= 0 && string_id < sizeof(ohci_rh_strings) / sizeof(ohci_rh_strings[0])) {
						int amount = MIN(ohci_rh_strings[string_id].s_len, req->req_length);
						memcpy(xfer->xfer_data, &ohci_rh_strings[string_id], amount);
						xfer->xfer_result_length = amount;
						err = ananas_success();
					}
					break;
				}
				case USB_DESCR_TYPE_CONFIG: {
					int amount = MIN(ohci_rh_config.d_config.cfg_totallen, req->req_length);
					memcpy(xfer->xfer_data, &ohci_rh_config, amount);
					xfer->xfer_result_length = amount;
					err = ananas_success();
					break;
				}
			}
			break;
		case USB_REQUEST_STANDARD_SET_ADDRESS:
			DPRINTF(dev, "set address: %d", req->req_value);
			err = ananas_success();
			break;
		case USB_REQUEST_STANDARD_SET_CONFIGURATION:
			DPRINTF(dev, "set config: %d", req->req_value);
			err = ananas_success();
			break;
		case USB_REQUEST_CLEAR_HUB_FEATURE:
			break;
		case USB_REQUEST_SET_HUB_FEATURE:
			break;
		case USB_REQUEST_GET_BUS_STATE:
			break;
		case USB_REQUEST_GET_HUB_DESCRIPTOR: {
			/* First step is to construct our hub descriptor */
			int port_len = (p->ohci_rh_numports + 7) / 8;
			struct USB_DESCR_HUB hd;
			memset(&hd, 0, sizeof(hd));
			hd.hd_length = sizeof(hd) - (HUB_MAX_PORTS + 7) / 8 + port_len;
			hd.hd_type = USB_DESCR_TYPE_HUB;
			hd.hd_numports = p->ohci_rh_numports;
			hd.hd_max_current = 0;

			uint32_t rhda = ohci_read4(dev, OHCI_HCRHDESCRIPTORA);
			hd.hd_flags = 0;
			if ((rhda & (OHCI_RHDA_NPS | OHCI_RHDA_PSM)) == OHCI_RHDA_PSM)
				hd.hd_flags |= USB_HD_FLAG_PS_INDIVIDUAL;
			if (rhda & OHCI_RHDA_NOCP)
				hd.hd_flags |= USB_HD_FLAG_OC_NONE;
			else if (rhda & OHCI_RHDA_OCPM)
				hd.hd_flags |= USB_HD_FLAG_OC_INDIVIDUAL;
			else
				hd.hd_flags |= USB_HD_FLAG_OC_GLOBAL;
			hd.hd_poweron2good = OHCI_RHDA_POTPGT(rhda);

			/* Fill out all removable bits */
			uint32_t rhdb = ohci_read4(dev, OHCI_HCRHDESCRIPTORB);
			for (unsigned int n = 1; n < p->ohci_rh_numports; n++) {
				if (rhdb & (1 << n))
					hd.hd_removable[n / 8] |= 1 << (n  & 7);
			}

			/* Copy the descriptor we just created */
			int amount = MIN(hd.hd_length, req->req_length);
			memcpy(xfer->xfer_data, &hd, amount);
			xfer->xfer_result_length = amount;
			err = ananas_success();
			break;
		}
		case USB_REQUEST_GET_HUB_STATUS: {
			if (req->req_value == 0 && req->req_index == 0 && req->req_length == 4) {
				uint32_t hs = 0;
				/* XXX over-current */
				memcpy(xfer->xfer_data, &hs, sizeof(hs));
				xfer->xfer_result_length = sizeof(hs);
				err = ananas_success();
			}
			break;
		}
		case USB_REQUEST_GET_PORT_STATUS: {
			if (req->req_value == 0 && req->req_index >= 1 && req->req_index <= p->ohci_rh_numports && req->req_length == 4) {
				uint32_t st = ohci_read4(dev, OHCI_HCRHPORTSTATUSx + (req->req_index - 1) * 4);

				struct USB_HUB_PORTSTATUS ps;
				memset(&ps, 0, sizeof(ps));
				ps.ps_portstatus = st & 0x31f;
				ps.ps_portchange = (st >> 16) & 0x1f;
				memcpy(xfer->xfer_data, &ps, sizeof(ps));
				xfer->xfer_result_length = sizeof(ps);
				err = ananas_success();
			}
			break;
		}
		case USB_REQUEST_SET_PORT_FEATURE: {
			unsigned int port = req->req_index;
			if (port >= 1 && port <= p->ohci_rh_numports) {
				port = OHCI_HCRHPORTSTATUSx + (req->req_index - 1) * 4;
				err = ananas_success();
				switch(req->req_value) {
					case HUB_FEATURE_PORT_RESET: {
						/*
						 * To reset a port, we'll enable the PRS bit. Once it's unset, the
						 * hub will be resetting the device and trigger PRSC (which the hub
						 * checks for)
						 */
						DPRINTF(dev, "set port reset, port %d", req->req_index);
						ohci_write4(dev, port, OHCI_RHPS_PRS);
						int n = 10;
						while (n > 0 && (ohci_read4(dev, port) & OHCI_RHPS_PRS) != 0) {
							delay(100);
							n--;
						}
						if (n == 0) {
							device_printf(dev, "port %u not responding to reset", n);
							err = ANANAS_ERROR(NO_DEVICE);
						}
						break;
					}
					case HUB_FEATURE_PORT_SUSPEND:
						DPRINTF(dev, "set port suspend, port %d", req->req_index);
						ohci_write4(dev, port, OHCI_RHPS_PSS);
						err = ananas_success();
						break;
					case HUB_FEATURE_PORT_POWER:
						DPRINTF(dev, "set port power, port %d", req->req_index);
						ohci_write4(dev, port, OHCI_RHPS_PPS);
						break;
					default:
						err = ANANAS_ERROR(BAD_OPERATION);
						break;
				}
			}
			break;
		}
		case USB_REQUEST_CLEAR_PORT_FEATURE: {
			unsigned int port = req->req_index;
			if (port >= 1 && port <= p->ohci_rh_numports) {
				port = OHCI_HCRHPORTSTATUSx + (req->req_index - 1) * 4;
				err = ananas_success();
				switch(req->req_value) {
					case HUB_FEATURE_PORT_ENABLE:
						DPRINTF(dev, "HUB_FEATURE_PORT_ENABLE: port %d", req->req_index);
						ohci_write4(dev, port, OHCI_RHPS_CCS);
						break;
					case HUB_FEATURE_PORT_SUSPEND:
						DPRINTF(dev, "HUB_FEATURE_PORT_SUSPEND: port %d", req->req_index);
						ohci_write4(dev, port, OHCI_RHPS_POCI);
						break;
					case HUB_FEATURE_PORT_POWER:
						DPRINTF(dev, "HUB_FEATURE_PORT_POWER: port %d", req->req_index);
						ohci_write4(dev, port, OHCI_RHPS_LSDA);
						break;
					case HUB_FEATURE_C_PORT_CONNECTION:
						DPRINTF(dev, "HUB_FEATURE_C_PORT_CONNECTION: port %d", req->req_index);
						ohci_write4(dev, port, OHCI_RHPS_CSC);
						/* Re-enable RHSC after clear is complete */
						ohci_write4(dev, OHCI_HCINTERRUPTENABLE, OHCI_IE_RHSC);
						break;
					case HUB_FEATURE_C_PORT_RESET:
						DPRINTF(dev, "HUB_FEATURE_C_PORT_RESET: port %d", req->req_index);
						ohci_write4(dev, port, OHCI_RHPS_PRSC);
						/* Re-enable RHSC after reset is complete; otherwise it keeps spamming */
						ohci_write4(dev, OHCI_HCINTERRUPTENABLE, OHCI_IE_RHSC);
						break;
					case HUB_FEATURE_C_PORT_ENABLE:
						DPRINTF(dev, "HUB_FEATURE_C_PORT_ENABLE: port %d", req->req_index);
						ohci_write4(dev, port, OHCI_RHPS_PESC);
						break;
					case HUB_FEATURE_C_PORT_SUSPEND:
						DPRINTF(dev, "HUB_FEATURE_C_PORT_SUSPEND: port %d", req->req_index);
						ohci_write4(dev, port, OHCI_RHPS_PSSC);
						break;
					case HUB_FEATURE_C_PORT_OVER_CURRENT:
						DPRINTF(dev, "HUB_FEATURE_C_PORT_OVER_CURRENT: port %d", req->req_index);
						ohci_write4(dev, port, OHCI_RHPS_OCIC);
						break;
					default:
						err = ANANAS_ERROR(BAD_OPERATION);
						break;
				}
			}
			break;
		}
		default:
			err = ANANAS_ERROR(BAD_TYPE);
			break;
	}

#undef MIN

	if (ananas_is_failure(err)) {
		kprintf("oroothub: error %d\n", err);
		xfer->xfer_flags |= TRANSFER_FLAG_ERROR;
	}

	/* Immediately mark the transfer as completed */
	usbtransfer_complete_locked(xfer);
	return err;
}

errorcode_t
oroothub_handle_transfer(device_t dev, struct USB_TRANSFER* xfer)
{
	switch(xfer->xfer_type) {
		case TRANSFER_TYPE_CONTROL:
			return oroothub_ctrl_xfer(dev, xfer);
		case TRANSFER_TYPE_INTERRUPT:
			/* Transfer has been added to the queue; no need to do anything else here */
			return ananas_success();
	}
	panic("unsupported transfer type %d", xfer->xfer_type);
}

void
oroothub_process_interrupt_transfers(struct USB_DEVICE* usb_dev)
{
	device_t dev = usb_dev->usb_bus->bus_hcd;
	auto p = static_cast<struct OHCI_PRIVDATA*>(dev->privdata);

	/* Walk through every port, hungry for updates... */
	uint8_t hub_update[2] = { 0, 0 }; /* max 15 ports + hub status itself = 16 bits */
	int num_updates = 0;
	for (unsigned int n = 1; n <= p->ohci_rh_numports; n++) {
		int index = OHCI_HCRHPORTSTATUSx + (n - 1) * 4;
		uint32_t st = ohci_read4(dev, index);
		if ((st >> 16) != 0) {
			/* A changed event was triggered - need to report this */
			hub_update[n / 8] |= 1 << (n % 8);
			num_updates++;
		}
	}

	if (num_updates > 0) {
		int update_len = (p->ohci_rh_numports + 1 /* hub */ + 7 /* round up */) / 8;

		/* Alter all entries in the transfer queue */
		mutex_lock(&usb_dev->usb_mutex);
		LIST_FOREACH_IP(&usb_dev->usb_transfers, pending, xfer, struct USB_TRANSFER) {
			if (xfer->xfer_type != TRANSFER_TYPE_INTERRUPT)
				continue;
			memcpy(&xfer->xfer_data, hub_update, update_len);
			xfer->xfer_result_length = update_len;
			usbtransfer_complete_locked(xfer);
		}
		mutex_unlock(&usb_dev->usb_mutex);
	}
}

void
ohci_roothub_irq(device_t dev)
{
	auto p = static_cast<struct OHCI_PRIVDATA*>(dev->privdata);
	sem_signal(&p->ohci_rh_semaphore);
}

static void
oroothub_thread(void* ptr)
{
	device_t dev = static_cast<device_t>(ptr);
	auto p = static_cast<struct OHCI_PRIVDATA*>(dev->privdata);
	KASSERT(p->ohci_roothub != NULL, "no root hub?");

	while(1) {
		/* Wait until we get a roothub interrupt; that should signal something happened */
		sem_wait_and_drain(&p->ohci_rh_semaphore);

		/*
		 * If we do not have anything in the interrupt queue, there is no need to
		 * bother checking as no one can handle yet - best to wait...
		 */
		oroothub_process_interrupt_transfers(p->ohci_roothub);
	}	
}

errorcode_t
oroothub_init(device_t dev)
{
	auto p = static_cast<struct OHCI_PRIVDATA*>(dev->privdata);

	sem_init(&p->ohci_rh_semaphore, 0);

	uint32_t rhda = ohci_read4(dev, OHCI_HCRHDESCRIPTORA);
	int numports = OHCI_RHDA_NDP(rhda);
	if (numports < 1 || numports > 15) {
		device_printf(dev, "invalid number of %d port(s) present", numports);
		return ANANAS_ERROR(NO_DEVICE);
	}
	p->ohci_rh_numports = numports;

	/*
	 * Note that there is no need to attach the root hub; usb-bus does this upon attaching
	 * to the HCD driver.
	 */
	return ananas_success();
}

void
oroothub_start(device_t dev)
{
	/*
	 * Launch the poll thread to handle the interrupt pipe requests; we can't do
	 * that from oroothub_init() because the usbbus doesn't exist at that point
	 * and we don't know the USB device either.
	 */
	auto p = static_cast<struct OHCI_PRIVDATA*>(dev->privdata);
	kthread_init(&p->ohci_rh_pollthread, "oroothub", &oroothub_thread, dev);
	thread_resume(&p->ohci_rh_pollthread);
}

/* vim:set ts=2 sw=2: */
