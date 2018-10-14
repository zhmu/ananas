#include <ananas/types.h>
#include "kernel/dev/pci.h"
#include "kernel/driver.h"
#include "kernel/irq.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/x86/io.h"
#include "ata.h"
#include "ata-controller.h"
#include "ata-pci.h"

TRACE_SETUP;

#define ATA_FREELIST_LENGTH 16

namespace ata {

Result
ATAPCI::Attach()
{
	// Enable busmastering; we need this to be able to do DMA
	pci_enable_busmaster(*this, 1);

	// Grab the PCI DMA I/O port, it is the same for all PCI IDE controllers
	void* res_io = d_ResourceSet.AllocateResource(Resource::RT_IO, 16);
	if (res_io == NULL)
		return RESULT_MAKE_FAILURE(ENODEV);
	uint32_t atapci_dma_io = (uint32_t)(uintptr_t)res_io;

	/* XXX This is crude - but PCI/APCI do not seem to advertise these addresses? */
	struct Resource {
		unsigned int io, io_alt, irq;
	} resources[] = {
		{ 0x1f0, 0x3f4, 14 },
		{ 0x170, 0x374, 15 },
		{ 0, 0, 0 }
	};

	for (Resource* r = &resources[0]; r->io != 0; r++) {
		ResourceSet rs;
		rs.AddResource(Resource(Resource::RT_IO, r->io, 16));
		rs.AddResource(Resource(Resource::RT_IO, r->io_alt, 16));
		rs.AddResource(Resource(Resource::RT_IO, atapci_dma_io, 16));
		rs.AddResource(Resource(Resource::RT_IRQ, r->irq, 0));

		auto atacl = static_cast<ata::ATAController*>(device_manager::CreateDevice("atacontroller", CreateDeviceProperties(*this, rs)));
		if (atacl == nullptr)
			continue;

		if (auto result = device_manager::AttachSingle(*atacl); result.IsFailure())
			delete atacl;
	}

	return Result::Success();
}

Result
ATAPCI::Detach()
{
	panic("detach");
	return Result::Success();
}

namespace {

struct ATAPCI_Driver : public Driver
{
	ATAPCI_Driver()
	 : Driver("atapci")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return "pcibus";
	}

	Device* CreateDevice(const CreateDeviceProperties& cdp) override
	{
		auto res = cdp.cdp_ResourceSet.GetResource(Resource::RT_PCI_ClassRev, 0);
		if (res == NULL)
			return nullptr;

		uint32_t classrev = res->r_Base;
		if (PCI_CLASS(classrev) != PCI_CLASS_STORAGE || PCI_SUBCLASS(classrev) != PCI_SUBCLASS_IDE)
			return nullptr; // Not an IDE storage device

		return new ATAPCI(cdp);
	}
};

} // unnamed namespace

REGISTER_DRIVER(ATAPCI_Driver)

} // namespace ata

/* vim:set ts=2 sw=2: */
