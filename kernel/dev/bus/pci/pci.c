#include "bus/pci.h"
#include "i386/io.h"
#include "device.h"
#include "lib.h"

extern uint32_t pci_read_config_l(uint32_t bus, uint32_t dev, uint32_t func, uint32_t reg);
extern struct DRIVER drv_pcibus;

static int
pci_attach(device_t dev)
{
	unsigned int bus;
	for (bus = 0; bus < PCI_MAX_BUSSES; bus++) {
		/*
	 	 * Attempt to obtain the vendor/device ID of device 0 on this bus. If it
	 	 * does not exist, we assume the bus doesn't exist.
		 */
		uint32_t dev_vendor = pci_read_config_l(bus, 0, 0, PCI_REG_DEVICEVENDOR);
		if (dev_vendor & 0xffff == PCI_NOVENDOR)
			continue;

		device_t new_bus = device_alloc(dev, &drv_pcibus);
		device_add_resource(new_bus, RESTYPE_PCI_BUS, bus, 0);
		device_add_resource(new_bus, RESTYPE_PCI_VENDORID, dev_vendor & 0xffff, 0);
		device_add_resource(new_bus, RESTYPE_PCI_DEVICEID, dev_vendor >> 16, 0);

		device_print_attachment(new_bus);
		device_attach_single(new_bus);
	}
	return 0;
}

struct DRIVER drv_pci = {
	.name					= "pci",
	.drv_probe		= NULL,
	.drv_attach		= pci_attach
};

DRIVER_PROBE(pci)
DRIVER_PROBE_BUS(pcihb)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
