#include "types.h"

#ifndef __DEVICE_H__
#define __DEVICE_H__

typedef struct DEVICE* device_t;
typedef struct DRIVER* driver_t;
typedef struct PROBE* probe_t;

/*
 *  This describes a device driver.
 */
struct DRIVER {
	int	(*drv_probe)(device_t);
	int	(*drv_attach)(device_t);
	ssize_t	(*drv_write)(device_t, const char*, size_t);
	ssize_t	(*drv_read)(device_t, char*, size_t);
};

/*
 * This describes a device; it is generally attached but this structure
 * is also used during probe / attach phase.
 */
struct DEVICE {
	/* Device's driver */
	driver_t*	driver;
};

/*
 * The Device Probe structure describes a possible relationship between a bus
 * and a device (i.e. device X may appear on bus Y). Once a bus is attaching,
 * it will use these declarations as an attempt to add all possible devices on
 * the bus.
 */
struct PROBE {
	/* Driver we are attaching */
	driver_t*	driver;

	/* Parent bus */
	const char*	bus;
};

#define DRIVER_PROBE(drv,bs) \
	static struct PROBE probe_##bus##_##drv = { \
		.driver = NULL, \
		.bus    = STRINGIFY(bs) \
	};

void device_init();

#endif /* __DEVICE_H__ */
