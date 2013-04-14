#include <machine/vm.h>
#include <ananas/x86/io.h>
#include <ananas/x86/pcihb.h>
#include <ananas/bus/pcihb.h>
#include <ananas/console.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/trace.h>
#include <ananas/vm.h>

TRACE_SETUP;

/*
 * This file implements an x86 PCI Host Bridge driver; such a driver is
 * responsible for hooking up the host CPU to the PCI bridge logic. It
 * will be probed by ACPI or using the plain old ISA bus.
 *
 * It must export pci_read_config() and pci_write_config() functions which
 * will be used by the architecture-independant PCI stack in dev/pci/
 */

uint32_t
pci_read_config(uint32_t bus, uint32_t dev, uint32_t func, uint32_t reg, int width)
{
	outl(PCI_CFG1_ADDR, PCI_MAKE_ADDR(bus, dev, func, reg));
	switch(width) {
		case 32: return inl(PCI_CFG1_DATA);
		case 16: return inw(PCI_CFG1_DATA);
		case  8: return inb(PCI_CFG1_DATA);
		default: panic("unsupported width %u", width);
	}
}

void
pci_write_config(uint32_t bus, uint32_t dev, uint32_t func, uint32_t reg, uint32_t value, int width)
{
	outl(PCI_CFG1_ADDR, PCI_MAKE_ADDR(bus, dev, func, reg));
	switch(width) {
		case 32: outb(PCI_CFG1_DATA, value); break;
		case 16: outw(PCI_CFG1_DATA, value); break;
		case  8: outl(PCI_CFG1_DATA, value); break;
		default: panic("unsupported width %u", width);
	}
}

static errorcode_t
pcihb_attach(device_t dev)
{
	void* res = device_alloc_resource(dev, RESTYPE_IO, 7);
	if (res == NULL)
		return ANANAS_ERROR(NO_RESOURCE);
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
