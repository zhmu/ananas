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
 *
 * Fields marked with [S] are static and won't change after initialization; fields
 * with [M] use the mutex to protect them.
 */
struct USB_DEVICE {
	mutex_t usb_mutex;
	struct USB_BUS*	usb_bus;			/* [S] Bus the device resides on */
	struct USB_HUB*	usb_hub;			/* [S] Hub the device is attached to */
	int		usb_port;			/* [S] Port of the hub the device is attached to */
	device_t	usb_device;			/* [S] Device reference */
	void*		usb_hcd_privdata;		/* [S] HCD data for the given device */
	int		usb_address;		   	/* Assigned USB address */
	int		usb_max_packet_sz0;		/* Maximum packet size for endpoint 0 */
#define USB_DEVICE_DEFAULT_MAX_PACKET_SZ0	8
	struct USB_INTERFACE usb_interface[USB_MAX_INTERFACES];
	int		usb_num_interfaces;
	int		usb_cur_interface;
	struct USB_DESCR_DEVICE usb_descr_device;
	struct USB_PIPES usb_pipes;			/* [M] */

	/* Pending transfers for this device */
	struct USB_TRANSFER_QUEUE usb_transfers;	/* [M] */

	/* Provide link fields for device list */
	LIST_FIELDS(struct USB_DEVICE);
};

#define USB_DEVICE_FLAG_LOW_SPEED (1 << 0)
#define USB_DEVICE_FLAG_ROOT_HUB (1 << 31)

struct USB_DEVICE* usb_alloc_device(struct USB_BUS* bus, struct USB_HUB* hub, int hub_port, int flags);

/* Attaches a single device (which doesn't have an address yet) */
errorcode_t usbdev_attach(struct USB_DEVICE* usb_dev);

/* Removes an USB device from the bus */
errorcode_t usbdev_detach(struct USB_DEVICE* usb_dev);

#endif /* __ANANAS_USB_DEVICE_H__ */
