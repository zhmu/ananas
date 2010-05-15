#include <sys/types.h>

#ifndef __DEVICE_H__
#define __DEVICE_H__

typedef struct DEVICE* device_t;
typedef struct DRIVER* driver_t;
typedef struct PROBE* probe_t;

#define DEVICE_MAX_RESOURCES	16
#define DEVICE_RESOURCE_NONE 0xffffffff

enum RESOURCE_TYPE {
	RESTYPE_UNUSED,
	/* Base resource types */
	RESTYPE_MEMORY,
	RESTYPE_IO,
	RESTYPE_IRQ,
	/* Generic child devices */
	RESTYPE_CHILDNUM,
	/* PCI-specific resource types */
	RESTYPE_PCI_VENDORID,
	RESTYPE_PCI_DEVICEID,
	RESTYPE_PCI_BUS,
	RESTYPE_PCI_DEVICE,
	RESTYPE_PCI_FUNCTION,
	RESTYPE_PCI_CLASS,
};

typedef enum RESOURCE_TYPE resource_type_t;

struct RESOURCE {
	resource_type_t	type;
	unsigned int	base;
	unsigned int	length;
};

/*
 *  This describes a device driver.
 */
struct DRIVER {
	char*   name;
	unsigned int current_unit;

	int	(*drv_probe)(device_t);
	int	(*drv_attach)(device_t);
	void	(*drv_attach_children)(device_t);
	ssize_t	(*drv_write)(device_t, const void*, size_t, off_t);
	ssize_t	(*drv_read)(device_t, void*, size_t, off_t);
	/* for block devices: enqueue request */
	void	(*drv_enqueue)(device_t, void*);
	/* for block devices: start request queue */
	void	(*drv_start)(device_t);
};

/*
 * This describes a device; it is generally attached but this structure
 * is also used during probe / attach phase.
 */
struct DEVICE {
	/* Next driver XXX enumeration kludge */
	struct	DEVICE*	next;

	/* Device's driver */
	driver_t	driver;

	/* Parent device */
	device_t	parent;

	/* Unit number */
	unsigned int	unit;

	/* Device resources */
	struct RESOURCE	resource[DEVICE_MAX_RESOURCES];

	/* Formatted name XXX */
	char		name[128 /* XXX */];

	/* Private data for the device driver */
	void*		privdata;
};

/*
 * The Device Probe structure describes a possible relationship between a bus
 * and a device (i.e. device X may appear on bus Y). Once a bus is attaching,
 * it will use these declarations as an attempt to add all possible devices on
 * the bus.
 */
struct PROBE {
	/* Driver we are attaching */
	driver_t	driver;

	/* Busses this device appears on */
	const char*	bus[];
};

#define DRIVER_PROBE(drvr) \
	struct PROBE probe_##drvr = { \
		.driver = &drv_##drvr, \
		.bus = {

#define DRIVER_PROBE_BUS(bus) \
			STRINGIFY(bus),

#define DRIVER_PROBE_END() \
			NULL \
		} \
	};

void device_init();
device_t device_alloc(device_t bus, driver_t drv);
void device_free(device_t dev);
int device_attach_single(device_t dev);

int device_get_resources_byhint(device_t dev, const char* hint, const char** hints);
int device_get_resources(device_t dev, const char** hints);
ssize_t	device_write(device_t dev, const char* buf, size_t len, off_t offset);
ssize_t	device_read(device_t dev, char* buf, size_t len, off_t offset);

void* device_alloc_resource(device_t dev, resource_type_t type, size_t len);

int device_add_resource(device_t dev, resource_type_t type, unsigned int base, unsigned int len);
struct RESOURCE* device_get_resource(device_t dev, resource_type_t type, int index);

void device_set_biodev(device_t dev, device_t biodev);
struct DEVICE* device_find(const char* name);

void device_printf(device_t dev, const char* fmt, ...);

#endif /* __DEVICE_H__ */
