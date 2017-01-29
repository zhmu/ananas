#ifndef __ANANAS_USB_BUS_H__
#define __ANANAS_USB_BUS_H__

#include <ananas/LIST.h>

struct USB_DEVICE;
struct USB_HUB;
LIST_DEFINE(USB_DEVICES, struct USB_DEVICE);

/*
 * Single USB bus - directly connected to a HCD.
 */
struct USB_BUS {
	device_t bus_dev;
	device_t bus_hcd;

#define USB_BUS_FLAG_NEEDS_EXPLORE (1 << 0)
	int bus_flags;

	/* List of all USB devices on this bus */
	struct USB_DEVICES bus_devices;

	/* Mutex protecting the bus */
	mutex_t bus_mutex;

	/* Link fields for attach or bus list */
	LIST_FIELDS(struct USB_BUS);
};

/* Initialize the usbbus kernel thread which handles device exploring */
void usbbus_init();

/* Allocates an USB address on the bus; returns 0 on failure */
int usbbus_alloc_address(struct USB_BUS* bus);

/* Frees an USB address on the bus */
void usbbus_free_address(struct USB_BUS* bus, int addr);

/* Called when an USB bus needs exploring (that is, devices needed to be attached/removed) */
void usbbus_schedule_explore(struct USB_BUS* bus);

/* Schedules a device for attachment */
void usbbus_schedule_attach(struct USB_DEVICE* dev);

/* To be called when a hub itself it going away - must be called with bus lock held */
errorcode_t usb_bus_detach_hub(struct USB_BUS* bus, struct USB_HUB* hub);

#endif /* __ANANAS_USB_BUS_H__ */
