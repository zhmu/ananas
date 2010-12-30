#include <machine/vm.h>
#include <ananas/x86/io.h>
#include <ananas/console.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/trace.h>
#include <ananas/vm.h>

TRACE_SETUP;

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

uint16_t
pci_read_config_w(uint32_t bus, uint32_t dev, uint32_t func, uint32_t reg)
{
	uint32_t addr = 0x80000000L | (bus << 16) | (dev << 11) | (func << 8) | reg;
	outl(pcihb_io, addr);
	return inw(pcihb_io + 4);
}

void
pci_write_config_w(uint32_t bus, uint32_t dev, uint32_t func, uint32_t reg, uint16_t value)
{
	uint32_t addr = 0x80000000L | (bus << 16) | (dev << 11) | (func << 8) | reg;
	outl(pcihb_io, addr);
	outw(pcihb_io + 4, value);
}

void
pci_write_config_l(uint32_t bus, uint32_t dev, uint32_t func, uint32_t reg, uint32_t value)
{
	uint32_t addr = 0x80000000L | (bus << 16) | (dev << 11) | (func << 8) | reg;
	outl(pcihb_io, addr);
	outl(pcihb_io + 4, value);
}

static errorcode_t
pcihb_attach(device_t dev)
{
	void* res = device_alloc_resource(dev, RESTYPE_IO, 7);
	if (res == NULL)
		return ANANAS_ERROR(NO_RESOURCE);
	pcihb_io = (uintptr_t)res;

	return ANANAS_ERROR_OK;
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
