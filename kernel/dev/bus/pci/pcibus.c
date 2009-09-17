#include "bus/pci.h"
#include "i386/io.h"
#include "device.h"
#include "lib.h"

extern uint32_t pci_read_config_l(uint32_t bus, uint32_t dev, uint32_t func, uint32_t reg);

static int
pcibus_attach(device_t dev)
{
	struct RESOURCE* res = device_get_resource(dev, RESTYPE_PCI_BUS, 0);
	KASSERT(res != NULL, "called without a PCI bus resource");

	unsigned int busno = res->base;
	unsigned int devno;
	for (devno = 0; devno < PCI_MAX_DEVICES; devno++) {
		/*
		 * Grab the device/vendor pair; if the vendor is PCI_NOVENDOR, there's no
		 * device here and we skip the device alltogether.
		 */
		uint32_t dev_vendor = pci_read_config_l(busno, devno, 0, PCI_REG_DEVICEVENDOR);
		if ((uint32_t)(dev_vendor & 0xffff) == PCI_NOVENDOR) {
			continue;
		}

		/*
		 * Fetch the header-type; this is used to figure out whether this is an
 		 * ordinary PCI device or not
		 */
		unsigned int max_func = 0;
		if (pci_read_config_l(busno, devno, 0, PCI_REG_HEADERTIMER) & PCI_HEADER_MULTI)
			max_func = PCI_MAX_FUNCS;

		unsigned int funcno;
		for (funcno = 0; funcno <= max_func; funcno++) {
			dev_vendor = pci_read_config_l(busno, devno, funcno, PCI_REG_DEVICEVENDOR);
			if ((uint32_t)(dev_vendor & 0xffff) == PCI_NOVENDOR)
				continue;

			/* Walk through the BAR registers */
			unsigned int bar;
			for (bar = PCI_REG_BAR0; bar <= PCI_REG_BAR5; bar += 4) {
				uint32_t res = pci_read_config_l(busno, devno, funcno, bar);

//#define  PCI_BAR_MMIO		0x01
			//	kprintf("%x=%x ",bar, res);
			}

			/*
			 * Retrieve the PCI device class; drivers may use this to determine whether
			 * they need to attach.
			 */
			uint32_t stat_cmd = pci_read_config_l(busno, devno, funcno, PCI_REG_STATUSCOMMAND);
			uint32_t class = pci_read_config_l(busno, devno, funcno, PCI_REG_CLASSREVISION);

			/* Create a new device and pollute it with PCI resources */
			device_t new_dev = device_alloc(dev, NULL);
			device_add_resource(new_dev, RESTYPE_PCI_BUS, busno, 0);
			device_add_resource(new_dev, RESTYPE_PCI_DEVICE, devno, 0);
			device_add_resource(new_dev, RESTYPE_PCI_FUNCTION, funcno, 0);
			device_add_resource(new_dev, RESTYPE_PCI_VENDORID, dev_vendor & 0xffff, 0);
			device_add_resource(new_dev, RESTYPE_PCI_DEVICEID, dev_vendor >> 16, 0);
			device_add_resource(new_dev, RESTYPE_PCI_CLASS, PCI_CLASS(class), 0);

			kprintf("%s%u: no match for vendor 0x%x device 0x%x class %u, device ignored\n",
				dev->name, dev->unit, dev_vendor & 0xffff, dev_vendor >> 16, PCI_CLASS(class));

			device_free(new_dev);
		}
	}
	return 0;
}

struct DRIVER drv_pcibus = {
	.name			= "pcibus",
	.drv_probe		= NULL,
	.drv_attach		= pcibus_attach
};

DRIVER_PROBE(pcibus)
/*
 * The 'pci' driver will attach each bus itself as needed. 
 *
 * DRIVER_PROBE_BUS(pci)
 */
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
