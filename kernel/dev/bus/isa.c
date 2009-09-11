#include "device.h"

struct DRIVER drv_isa = {
	.name				= "isa",
	.drv_probe	= NULL,
	.drv_attach	= NULL,
	.drv_write	= NULL
};

DRIVER_PROBE(isa)
DRIVER_PROBE_BUS(corebus)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
