#include <ananas/bus/pci.h>
#include <ananas/device.h>
#include <ananas/driver.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <machine/pcihb.h>

namespace {

class PCIBus : public Ananas::Device, private Ananas::IDeviceOperations
{
public:
	using Device::Device;
	virtual ~PCIBus() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	errorcode_t Attach() override;
	errorcode_t Detach() override;

};

errorcode_t
PCIBus::Attach()
{
	auto busResource = d_ResourceSet.GetResource(Ananas::Resource::RT_PCI_Bus, 0);
	KASSERT(busResource != nullptr, "called without a PCI bus resource");

	unsigned int busno = busResource->r_Base;
	for (unsigned int devno = 0; devno < PCI_MAX_DEVICES; devno++) {
		uint32_t dev_vendor = pci_read_config(busno, devno, 0, PCI_REG_DEVICEVENDOR, 32);
		if ((uint32_t)(dev_vendor & 0xffff) == PCI_NOVENDOR)
			continue; /* nothing here */

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
				continue; /* nothing here */

			/*
			 * Retrieve the PCI device class; drivers may use this to determine whether
			 * they need to attach.
			 */
			uint32_t class_revision = pci_read_config(busno, devno, funcno, PCI_REG_CLASSREVISION, 32);

			// Collect the PCI resources
			Ananas::ResourceSet resourceSet;
			resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_PCI_Bus, busno, 0));
			resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_PCI_Device, devno, 0));
			resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_PCI_Function, funcno, 0));
			resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_PCI_VendorID, dev_vendor & 0xffff, 0));
			resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_PCI_DeviceID, dev_vendor >> 16, 0));
			resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_PCI_ClassRev, class_revision, 0));

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
						resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_IO, mmio, len));
					}
				} else {
					/* memory space */
					addr_t mem = res & 0xfffffff0;
					len = 0xfffffff0 & len;
					if (mem != 0 && len != 0) {
						len = (len & ~(len - 1)) - 1;
						resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_Memory, mem, len));
					}
				}
			}

			/* Fetch the IRQ line, if any */
			uint32_t irq = pci_read_config(busno, devno, funcno, PCI_REG_INTERRUPT, 32) & 0xff;
			if (irq != 0 && irq != 0xff)
				resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_IRQ, irq, 0));

			/* Attempt to attach this new child device */
			if (Ananas::DeviceManager::AttachChild(*this, resourceSet) != nullptr)
				continue;
#if 0
			Printf("no match for vendor 0x%x device 0x%x class %u, device ignored",
				dev_vendor & 0xffff, dev_vendor >> 16, PCI_CLASS(class_revision));
#endif
		}
	}
	return ananas_success();
}

errorcode_t
PCIBus::Detach()
{
	return ananas_success();
}

struct PCIBus_Driver : public Ananas::Driver
{
	PCIBus_Driver()
	 : Driver("pcibus")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return nullptr;
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		return new PCIBus(cdp);
	}
};

} // unnamed namespace

REGISTER_DRIVER(PCIBus_Driver)

/* vim:set ts=2 sw=2: */
