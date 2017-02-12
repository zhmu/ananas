#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <ananas/types.h>
#include <ananas/list.h>
#include <ananas/lock.h>
#include <ananas/init.h>
#include <ananas/cdefs.h>
#include <ananas/resourceset.h>

typedef struct DEVICE* device_t;
typedef struct DRIVER* driver_t;
typedef struct PROBE* probe_t;

struct BIO;
struct USB_BUS;
struct USB_DEVICE;
struct USB_TRANSFER;

/*
 *  This describes a device driver.
 */
struct DRIVER {
	const char*   	name;
	unsigned int	current_unit;

	errorcode_t	(*drv_probe)(device_t);
	errorcode_t	(*drv_attach)(device_t);
	errorcode_t	(*drv_detach)(device_t);
	void		(*drv_dump)(device_t); /* debug dump */
	void		(*drv_attach_children)(device_t);
	errorcode_t	(*drv_write)(device_t, const void*, size_t*, off_t);
	errorcode_t	(*drv_bwrite)(device_t, struct BIO*);
	errorcode_t	(*drv_read)(device_t, void*, size_t*, off_t);
	errorcode_t	(*drv_bread)(device_t, struct BIO*);
	errorcode_t	(*drv_stat)(device_t, void*);
	errorcode_t	(*drv_devctl)(device_t, process_t*, unsigned int, void*, size_t);
	/* for block devices: enqueue request */
	void		(*drv_enqueue)(device_t, void*);
	/* for block devices: start request queue */
	void		(*drv_start)(device_t);
	/* USB */
	errorcode_t	(*drv_usb_setup_xfer)(device_t, struct USB_TRANSFER*);
	errorcode_t	(*drv_usb_schedule_xfer)(device_t, struct USB_TRANSFER*);
	errorcode_t	(*drv_usb_cancel_xfer)(device_t, struct USB_TRANSFER*);
	errorcode_t	(*drv_usb_teardown_xfer)(device_t, struct USB_TRANSFER*);
	void*		(*drv_usb_hcd_initprivdata)(int);
	void		(*drv_usb_set_roothub)(device_t, struct USB_DEVICE*);
	errorcode_t	(*drv_roothub_xfer)(device_t, struct USB_TRANSFER*);
	void		(*drv_usb_explore)(struct USB_DEVICE*);
};

/*
 * This describes a device; it is generally attached but this structure
 * is also used during probe / attach phase.
 */
struct DEVICE {
	/* Device's driver */
	driver_t	driver;

	/* Parent device */
	device_t	parent;

	/* DMA tag assigned to the device */
	dma_tag_t	dma_tag;

	/* Unit number */
	unsigned int	unit;

	/* Device resources */
	Ananas::ResourceSet d_resourceset;

	/* Formatted name XXX */
	char		name[128 /* XXX */];

	/* Private data for the device driver */
	void*		privdata;

	/* Waiters */
	semaphore_t	waiters;

	/* Queue fields */
	LIST_FIELDS(struct DEVICE);
};
LIST_DEFINE(DEVICE_QUEUE, struct DEVICE);

/*
 * The Device Probe structure describes a possible relationship between a bus
 * and a device (i.e. device X may appear on bus Y). Once a bus is attaching,
 * it will use these declarations as an attempt to add all possible devices on
 * the bus.
 */
struct PROBE {
	/* Driver we are attaching */
	driver_t	driver;

	/* Queue fields */
	LIST_FIELDS(struct PROBE);

	/* Busses this device appears on */
	const char*	busses;
};
LIST_DEFINE(DEVICE_PROBE, struct PROBE);

#define DRIVER_PROBE(drvr) \
	extern struct PROBE probe_##drvr; \
	static errorcode_t register_##drvr() { \
		return device_register_probe(&probe_##drvr); \
	}; \
	static errorcode_t unregister_##drvr() { \
		return device_unregister_probe(&probe_##drvr); \
	}; \
	INIT_FUNCTION(register_##drvr, SUBSYSTEM_DEVICE, ORDER_MIDDLE); \
	EXIT_FUNCTION(unregister_##drvr); \
	struct PROBE probe_##drvr = { \
		.driver = &drv_##drvr, \
		.busses =

#define DRIVER_PROBE_BUS(bus) \
			STRINGIFY(bus) ","

#define DRIVER_PROBE_END() \
	};

device_t device_alloc(device_t bus, driver_t drv);
void device_free(device_t dev);
errorcode_t device_attach_single(device_t dev);
errorcode_t device_attach_child(device_t dev);
void device_attach_bus(device_t bus);
device_t device_clone(device_t dev);
errorcode_t device_detach(device_t dev);

errorcode_t device_register_probe(struct PROBE* p);
errorcode_t device_unregister_probe(struct PROBE* p);

errorcode_t device_write(device_t dev, const void* buf, size_t* len, off_t offset);
errorcode_t device_bwrite(device_t dev, struct BIO* bio);
errorcode_t device_read(device_t dev, void* buf, size_t* len, off_t offset);
errorcode_t device_bread(device_t dev, struct BIO* bio);

struct DEVICE* device_find(const char* name);
struct DEVICE_QUEUE* device_get_queue();

void device_printf(device_t dev, const char* fmt, ...);

void device_waiter_add_head(device_t dev, struct THREAD* thread);
void device_waiter_add_tail(device_t dev, struct THREAD* thread);
void device_waiter_signal(device_t dev);

// Device-specific resource management; assigns the resource to the device
void* operator new(size_t len, device_t dev) throw();
void operator delete(void* p, device_t dev) throw();
void* operator new[](size_t len, device_t dev) throw();
void operator delete[](void* p, device_t dev) throw();

#endif /* __DEVICE_H__ */
