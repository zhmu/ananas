#ifndef __ANANAS_USB_DEVICE_H__
#define __ANANAS_USB_DEVICE_H__

#include <ananas/bus/usb/core.h>

struct USB_BUS;
struct USB_HUB;
struct USB_TRANSFER;

/*
 * An USB device consists of a name, an address, pipes and a driver. We
 * augment the existing device for this and consider the USB_DEVICE as the
 * private data for the device in question.
 */
struct USB_DEVICE {
	struct USB_BUS*	usb_bus;			/* Bus the device resides on */
	struct USB_HUB*	usb_hub;			/* Hub the device is attached to */
	device_t	usb_device;			/* Device reference */
	void*		usb_hcd_privdata;		/* HCD data for the given device */
	int		usb_address;		   	/* Assigned USB address */
	int		usb_max_packet_sz0;		/* Maximum packet size for endpoint 0 */
#define USB_DEVICE_DEFAULT_MAX_PACKET_SZ0	8
	struct USB_INTERFACE usb_interface[USB_MAX_INTERFACES];
	int		usb_num_interfaces;
	int		usb_cur_interface;
	struct USB_DESCR_DEVICE usb_descr_device;
	struct USB_PIPES usb_pipes;

	/* Pending transfers for this device */
	struct USB_TRANSFER_QUEUE usb_transfers;

	/* Provide link fields for device list */
	DQUEUE_FIELDS(struct USB_DEVICE);
};

#define USB_DEVICE_FLAG_LOW_SPEED (1 << 0)
#define USB_DEVICE_FLAG_ROOT_HUB (1 << 31)

struct USB_DEVICE* usb_alloc_device(struct USB_BUS* bus, struct USB_HUB* hub, int flags);

/* Attaches a single device (which doesn't have an address yet) */
errorcode_t usbdev_attach(struct USB_DEVICE* usb_dev);

#endif /* __ANANAS_USB_DEVICE_H__ */
