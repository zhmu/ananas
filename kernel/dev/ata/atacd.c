#include <machine/io.h>
#include <sys/dev/ata.h>
#include <sys/bio.h>
#include <sys/lib.h>
#include <mbr.h>

static int
atacd_attach(device_t dev)
{
	void* res = device_alloc_resource(dev, RESTYPE_CHILDNUM, 0);
	struct ATA_IDENTIFY* identify = (struct ATA_IDENTIFY*)dev->privdata;

	device_printf(dev, "<%s>", identify->model);

	return 1;
}

struct DRIVER drv_atacd = {
	.name					= "atacd",
	.drv_probe		= NULL,
	.drv_attach		= atacd_attach
};

/* vim:set ts=2 sw=2: */
