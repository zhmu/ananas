#include <ananas/x86/io.h>
#include <ananas/bus/pci.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <machine/pcihb.h>

Ananas::Device* pcibus_CreateDevice(const Ananas::CreateDeviceProperties& cdp);

namespace {

class PCI : public Ananas::Device, private Ananas::IDeviceOperations
{
public:
	using Device::Device;
	virtual ~PCI() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	errorcode_t Attach() override;
	errorcode_t Detach() override;
};

errorcode_t
PCI::Attach()
{
	for (unsigned int bus = 0; bus < PCI_MAX_BUSSES; bus++) {
		/*
	 	 * Attempt to obtain the vendor/device ID of device 0 on this bus. If it
	 	 * does not exist, we assume the bus doesn't exist.
		 */
		uint32_t dev_vendor = pci_read_config(bus, 0, 0, PCI_REG_DEVICEVENDOR, 32);
		if ((dev_vendor & 0xffff) == PCI_NOVENDOR)
			continue;

		Ananas::ResourceSet resourceSet;
		resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_PCI_Bus, bus, 0));
		resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_PCI_VendorID, dev_vendor & 0xffff, 0));
		resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_PCI_DeviceID, dev_vendor >> 16, 0));

		static int pcibus_unit = 0; // XXX
		Ananas::Device* new_bus = pcibus_CreateDevice(Ananas::CreateDeviceProperties(*this, "pcibus", pcibus_unit++, resourceSet));
		if (new_bus != NULL)
			Ananas::DeviceManager::AttachSingle(*new_bus);
	}
	return ananas_success();
}

errorcode_t
PCI::Detach()
{
	return ananas_success();
}

Ananas::Device*
pci_CreateDevice(const Ananas::CreateDeviceProperties& cdp)
{
	return new PCI(cdp);
}

} // unnamed namespace

DRIVER_PROBE(pci, "pci", pci_CreateDevice)
DRIVER_PROBE_BUS(pcihb)
DRIVER_PROBE_BUS(acpi-pcihb)
DRIVER_PROBE_END()

void
pci_write_cfg(Ananas::Device& device, uint32_t reg, uint32_t val, int size)
{
	Ananas::ResourceSet& resourceSet = device.d_ResourceSet;
	auto bus_res = resourceSet.GetResource(Ananas::Resource::RT_PCI_Bus, 0);
	auto dev_res = resourceSet.GetResource(Ananas::Resource::RT_PCI_Device, 0); 
	auto func_res = resourceSet.GetResource(Ananas::Resource::RT_PCI_Function, 0); 
	KASSERT(bus_res != NULL && dev_res != NULL && func_res != NULL, "missing pci resources");

	pci_write_config(bus_res->r_Base, dev_res->r_Base, func_res->r_Base, reg, val, size);
}

uint32_t
pci_read_cfg(Ananas::Device& device, uint32_t reg, int size)
{
	Ananas::ResourceSet& resourceSet = device.d_ResourceSet;
	auto bus_res = resourceSet.GetResource(Ananas::Resource::RT_PCI_Bus, 0);
	auto dev_res = resourceSet.GetResource(Ananas::Resource::RT_PCI_Device, 0); 
	auto func_res = resourceSet.GetResource(Ananas::Resource::RT_PCI_Function, 0); 
	KASSERT(bus_res != NULL && dev_res != NULL && func_res != NULL, "missing pci resources");

	return pci_read_config(bus_res->r_Base, dev_res->r_Base, func_res->r_Base, reg, size);
}

void
pci_enable_busmaster(Ananas::Device& device, bool on)
{
	uint32_t cmd = pci_read_cfg(device, PCI_REG_STATUSCOMMAND, 32);
	if (on)
		cmd |= PCI_CMD_BM;
	else
		cmd &= ~PCI_CMD_BM;
	pci_write_cfg(device, PCI_REG_STATUSCOMMAND, cmd, 32);
}

/* vim:set ts=2 sw=2: */
