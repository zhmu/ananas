#include <ananas/x86/io.h>
#include <ananas/bus/pci.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <machine/pcihb.h>

extern struct DRIVER drv_pcibus;

static errorcode_t
pci_attach(device_t dev)
{
	for (unsigned int bus = 0; bus < PCI_MAX_BUSSES; bus++) {
		/*
	 	 * Attempt to obtain the vendor/device ID of device 0 on this bus. If it
	 	 * does not exist, we assume the bus doesn't exist.
		 */
		uint32_t dev_vendor = pci_read_config(bus, 0, 0, PCI_REG_DEVICEVENDOR, 32);
		if ((dev_vendor & 0xffff) == PCI_NOVENDOR)
			continue;

		device_t new_bus = device_alloc(dev, &drv_pcibus);

		Ananas::ResourceSet& resourceSet = new_bus->d_resourceset;
		resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_PCI_Bus, bus, 0));
		resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_PCI_VendorID, dev_vendor & 0xffff, 0));
		resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_PCI_DeviceID, dev_vendor >> 16, 0));

		device_attach_single(new_bus);
	}
	return ananas_success();
}

void
pci_write_cfg(device_t dev, uint32_t reg, uint32_t val, int size)
{
	Ananas::ResourceSet& resourceSet = dev->d_resourceset;
	auto bus_res = resourceSet.GetResource(Ananas::Resource::RT_PCI_Bus, 0);
	auto dev_res = resourceSet.GetResource(Ananas::Resource::RT_PCI_Device, 0); 
	auto func_res = resourceSet.GetResource(Ananas::Resource::RT_PCI_Function, 0); 
	KASSERT(bus_res != NULL && dev_res != NULL && func_res != NULL, "missing pci resources");

	pci_write_config(bus_res->r_Base, dev_res->r_Base, func_res->r_Base, reg, val, size);
}

uint32_t
pci_read_cfg(device_t dev, uint32_t reg, int size)
{
	Ananas::ResourceSet& resourceSet = dev->d_resourceset;
	auto bus_res = resourceSet.GetResource(Ananas::Resource::RT_PCI_Bus, 0);
	auto dev_res = resourceSet.GetResource(Ananas::Resource::RT_PCI_Device, 0); 
	auto func_res = resourceSet.GetResource(Ananas::Resource::RT_PCI_Function, 0); 
	KASSERT(bus_res != NULL && dev_res != NULL && func_res != NULL, "missing pci resources");

	return pci_read_config(bus_res->r_Base, dev_res->r_Base, func_res->r_Base, reg, size);
}

void
pci_enable_busmaster(device_t dev, int on)
{
	uint32_t cmd = pci_read_cfg(dev, PCI_REG_STATUSCOMMAND, 32);
	if (on)
		cmd |= PCI_CMD_BM;
	else
		cmd &= ~PCI_CMD_BM;
	pci_write_cfg(dev, PCI_REG_STATUSCOMMAND, cmd, 32);
}

struct DRIVER drv_pci = {
	.name					= "pci",
	.drv_probe		= NULL,
	.drv_attach		= pci_attach
};

DRIVER_PROBE(pci)
DRIVER_PROBE_BUS(pcihb)
DRIVER_PROBE_BUS(acpi-pcihb)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
