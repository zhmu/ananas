#include <ananas/bus/pci.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <machine/pcihb.h>

extern struct PROBE* devprobe[];

static errorcode_t
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
		uint32_t dev_vendor = pci_read_config(busno, devno, 0, PCI_REG_DEVICEVENDOR, 32);
		if ((uint32_t)(dev_vendor & 0xffff) == PCI_NOVENDOR) {
			continue;
		}

		/*
		 * Fetch the header-type; this is used to figure out whether this is an
 		 * ordinary PCI device or not
		 */
		unsigned int max_func = 0;
		if (pci_read_config(busno, devno, 0, PCI_REG_HEADERTIMER, 32) & PCI_HEADER_MULTI)
			max_func = PCI_MAX_FUNCS;

		for (unsigned int funcno = 0; funcno <= max_func; funcno++) {
			dev_vendor = pci_read_config(busno, devno, funcno, PCI_REG_DEVICEVENDOR, 32);
			if ((uint32_t)(dev_vendor & 0xffff) == PCI_NOVENDOR)
				continue;

			/*
			 * Retrieve the PCI device class; drivers may use this to determine whether
			 * they need to attach.
			 */
			uint32_t class_revision = pci_read_config(busno, devno, funcno, PCI_REG_CLASSREVISION, 32);

			/* Create a new device and pollute it with PCI resources */
			device_t new_dev = device_alloc(dev, NULL);
			device_add_resource(new_dev, RESTYPE_PCI_BUS, busno, 0);
			device_add_resource(new_dev, RESTYPE_PCI_DEVICE, devno, 0);
			device_add_resource(new_dev, RESTYPE_PCI_FUNCTION, funcno, 0);
			device_add_resource(new_dev, RESTYPE_PCI_VENDORID, dev_vendor & 0xffff, 0);
			device_add_resource(new_dev, RESTYPE_PCI_DEVICEID, dev_vendor >> 16, 0);
			device_add_resource(new_dev, RESTYPE_PCI_CLASSREV, class_revision, 0);

			/* Walk through the BAR registers */
			for (unsigned int bar = PCI_REG_BAR0; bar <= PCI_REG_BAR5; bar += 4) {
				uint32_t res = pci_read_config(busno, devno, funcno, bar, 32);
				if (res == 0)
					continue;

				/*
				 * First of all, attempt to figure out the size of the resource;
				 * this can be done by writing all ones to the BAR register and
				 * re-reading it again (the PCI device will have forced all
			 	 * address bits to zero); if we invert the value we've read,
				 * we'll have obtained the length - we have to take the resource's
				 * length into account while doing this.
				 *
				 * Note that we must restore the value once done with it.
				 */
				pci_write_config(busno, devno, funcno, bar, 0xffffffff, 32);
				uint32_t len = pci_read_config(busno, devno, funcno, bar, 32);
				pci_write_config(busno, devno, funcno, bar, res, 32);
				if (len == 0 || len == 0xffffffff)
					continue;

				/* This appears to be a real resource; need to add it */
				if (res & PCI_BAR_MMIO) {
					/* memory mapped I/O space */
					uint32_t mmio = res & 0xfffffffc;
					len = 0xfffffffc & len;
					if (mmio != 0 && len != 0) {
						len = (len & ~(len - 1)) - 1;
						device_add_resource(new_dev, RESTYPE_IO, mmio, len);
					}
				} else {
					/* memory space */
					addr_t mem = res & 0xfffffff0;
					len = 0xfffffff0 & len;
					if (mem != 0 && len != 0) {
						len = (len & ~(len - 1)) - 1;
						device_add_resource(new_dev, RESTYPE_MEMORY, mem, len);
					}
				}
			}

			/* Fetch the IRQ line, if any */
			uint32_t irq = pci_read_config(busno, devno, funcno, PCI_REG_INTERRUPT, 32) & 0xff;
			if (irq != 0 && irq != 0xff)
				device_add_resource(new_dev, RESTYPE_IRQ, irq, 0);

			/*
			 * Walk through any device that attached on our bus, and see if it works
			 * XXX This is a kludge; the device stuff should do this
			 */
			extern struct DEVICE_PROBE probe_queue;

			int device_attached = 0;
			LIST_FOREACH(&probe_queue, p, struct PROBE) {
				/* See if the device lives on our bus */
				int exists = 0;
				for (const char** curbus = p->bus; *curbus != NULL; curbus++) {
					if (strcmp(*curbus, dev->name) == 0) {
						exists = 1;
						break;
					}
				}
				if (!exists)
					continue;

				/* This device may work - give it a chance to attach */
				new_dev->driver = p->driver;
				strcpy(new_dev->name, new_dev->driver->name);
				new_dev->unit = new_dev->driver->current_unit++;
				errorcode_t err = device_attach_single(new_dev);
				if (err == ANANAS_ERROR_NONE) {
					/* This worked; use the next unit for the new device */
					device_attached++;
					break;
				} else {
					/* No luck, revert the unit number */
					new_dev->driver->current_unit--;
				}
			}

			if (!device_attached) {
#if 0
				kprintf("%s%u: no match for vendor 0x%x device 0x%x class %u, device ignored\n",
					dev->name, dev->unit, dev_vendor & 0xffff, dev_vendor >> 16, PCI_CLASS(class_revision));
#endif
				new_dev->driver = NULL;
				device_free(new_dev);
			}
		}
	}
	return ANANAS_ERROR_NONE;
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
