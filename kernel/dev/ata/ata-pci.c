#include <ananas/types.h>
#include <ananas/dev/ata.h>
#include <ananas/bus/pci.h>
#include <ananas/bio.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/irq.h>
#include <ananas/mm.h>
#include <ananas/trace.h>
#include <ananas/lib.h>

TRACE_SETUP;

extern struct DRIVER drv_atapci;

static errorcode_t
atapci_probe(device_t dev)
{
	struct RESOURCE* res = device_get_resource(dev, RESTYPE_PCI_CLASSREV, 0);
	uint32_t classrev = res->base;
	/* Reject anything not a IDE storage device */
	if (PCI_CLASS(classrev) != PCI_CLASS_STORAGE || PCI_SUBCLASS(classrev) != PCI_SUBCLASS_IDE)
		return ANANAS_ERROR(NO_RESOURCE);
	return ANANAS_ERROR_OK;
}

static errorcode_t
atapci_attach(device_t dev)
{
	/* XXX This is crude - but PCI doesn't seem to advertise these addresses? */
	errorcode_t err = ANANAS_ERROR(NO_RESOURCE);
	switch(dev->unit) {
		case 0: err = ata_attach(dev, 0x1f0, 14); break;
		case 1: err = ata_attach(dev, 0x170, 15); break;
	}
	ANANAS_ERROR_RETURN(err);

	/* Device initialization went OK; now we should initialize the PCI DMA aspects */
	void* res_io = device_alloc_resource(dev, RESTYPE_IO, 16);
	if (res_io == NULL)
		return ANANAS_ERROR(NO_RESOURCE);

	struct ATA_PRIVDATA* privdata = dev->privdata;
	privdata->atapci.atapci_io = (uint32_t)(uintptr_t)res_io;

	return ANANAS_ERROR_NONE;
}

static void
atapci_attach_children(device_t dev)
{
	ata_attach_children(dev);

	/*
	 * We will find only a single atapci card even if there are two channels; to
	 * ensure we'll attach the other channel as well, forcefully attach ourselves
	 * to the other channel, too.
	 */
	if (dev->unit == 0) {
		device_t new_dev = device_clone(dev);
		device_attach_single(new_dev);
	}
}

struct DRIVER drv_atapci = {
	.name					= "atapci",
	.drv_probe		= atapci_probe,
	.drv_attach		= atapci_attach,
	.drv_attach_children		= atapci_attach_children,
	.drv_enqueue	= ata_enqueue,
	.drv_start		= ata_start
};

DRIVER_PROBE(atapci)
DRIVER_PROBE_BUS(pcibus)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
