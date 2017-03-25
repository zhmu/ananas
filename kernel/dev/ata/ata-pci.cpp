#include <ananas/types.h>
#include <ananas/bus/pci.h>
#include <ananas/error.h>
#include <ananas/driver.h>
#include <ananas/irq.h>
#include <ananas/mm.h>
#include <ananas/trace.h>
#include <ananas/lib.h>
#include <ananas/x86/io.h>
#include <machine/vm.h>
#include "ata.h"
#include "ata-controller.h"
#include "ata-pci.h"

TRACE_SETUP;

#define ATA_FREELIST_LENGTH 16

namespace Ananas {
namespace ATA {

errorcode_t
ATAPCI::Attach()
{
	// Enable busmastering; we need this to be able to do DMA
	pci_enable_busmaster(*this, 1);

	// Grab the PCI DMA I/O port, it is the same for all PCI IDE controllers
	void* res_io = d_ResourceSet.AllocateResource(Ananas::Resource::RT_IO, 16);
	if (res_io == NULL)
		return ANANAS_ERROR(NO_RESOURCE);
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
		Ananas::ResourceSet rs;
		rs.AddResource(Ananas::Resource(Ananas::Resource::RT_IO, r->io, 16));
		rs.AddResource(Ananas::Resource(Ananas::Resource::RT_IO, r->io_alt, 16));
		rs.AddResource(Ananas::Resource(Ananas::Resource::RT_IO, atapci_dma_io, 16));
		rs.AddResource(Ananas::Resource(Ananas::Resource::RT_IRQ, r->irq, 0));

		auto atacl = static_cast<Ananas::ATA::ATAController*>(Ananas::DeviceManager::CreateDevice("atacontroller", Ananas::CreateDeviceProperties(*this, rs)));
		if (atacl == nullptr)
			continue;

		if (ananas_is_failure(Ananas::DeviceManager::AttachSingle(*atacl)))
			delete atacl;
	}
#if 0
	int irq;
	switch(d_Unit) {
		case 0: atapci_io = 0x1f0; atapci_io_alt = 0x3f4; irq = 14; break;
		case 1: atapci_io = 0x170; atapci_io_alt = 0x374; irq = 15; break;
		default: return ANANAS_ERROR(NO_RESOURCE);
	}
#endif

	return ananas_success();
}

errorcode_t
ATAPCI::Detach()
{
	panic("detach");
	return ananas_success();
}

namespace {

struct ATAPCI_Driver : public Ananas::Driver
{
	ATAPCI_Driver()
	 : Driver("atapci")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return "pcibus";
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		auto res = cdp.cdp_ResourceSet.GetResource(Ananas::Resource::RT_PCI_ClassRev, 0);
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

} // namespace ATA
} // namespace Ananas


/* vim:set ts=2 sw=2: */
