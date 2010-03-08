#include "machine/vm.h"
#include "machine/io.h"
#include "console.h"
#include "device.h"
#include "lib.h"
#include "vm.h"

/*
 * This file implements a PCI Host Bridge driver; such a driver is responsible
 * for hooking up the host CPU to the PCI bridge logic.
 *
 * It must export pci_read_config() and pci_write_config() functions which
 * will be used by the architecture-independant PCI stack in dev/pci/
 *
 * Also, if a PCI bus exists, this file must instantiate it.
 *
 */
static uint32_t pcihb_io = 0;

uint32_t
pci_read_config_l(uint32_t bus, uint32_t dev, uint32_t func, uint32_t reg)
{
	uint32_t addr = 0x80000000L | (bus << 16) | (dev << 11) | (func << 8) | reg;
	outl(pcihb_io, addr);
	return inl(pcihb_io + 4);
}

static int
pcihb_attach(device_t dev)
{
	void* res = device_alloc_resource(dev, RESTYPE_IO, 7);
	if (res == NULL)
		return 1; /* XXX */
	pcihb_io = (uintptr_t)res;

	return 0;
}

struct DRIVER drv_pcihb = {
	.name       = "pcihb",
	.drv_probe  = NULL,
	.drv_attach = pcihb_attach,
};

DRIVER_PROBE(pcihb)
DRIVER_PROBE_BUS(isa)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
