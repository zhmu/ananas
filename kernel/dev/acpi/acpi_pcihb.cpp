#include "acpica/acpi.h"
#include "acpi.h"
#include "acpi_resource.h"
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/irq.h"
#include "kernel/result.h"
#include "kernel/trace.h"

TRACE_SETUP;

namespace {

constexpr uint16_t ADRToDeviceNumber(uint32_t adr)
{
	return (adr >> 16) & 0xffff;
}

template<typename Func>
void WalkPCIResourceTable(const ACPI_BUFFER& prt, Func f)
{
	if (prt.Pointer == nullptr)
		return;

	for (auto entry = static_cast<const ACPI_PCI_ROUTING_TABLE*>(prt.Pointer); entry->Length != 0; entry = reinterpret_cast<const ACPI_PCI_ROUTING_TABLE*>(reinterpret_cast<const char*>(entry) + entry->Length)) {
		if (f(*entry))
			break;
	}
}

struct ACPI_PCIHB : public Device, private IDeviceOperations, private IBusOperations
{
	using Device::Device;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	IBusOperations& GetBusDeviceOperations() override {
		return *this;
	}

	Result Attach() override
	{
		auto res = d_ResourceSet.GetResource(Resource::RT_ACPI_HANDLE, 0);
		if (res == nullptr)
			return RESULT_MAKE_FAILURE(EIO);
		auto acpiHandle = static_cast<ACPI_HANDLE*>(reinterpret_cast<void*>(res->r_Base));

		// Grab the PCI Routing Table (PRT); on-board devices tend to have their
		// own designated interrupts no longer shared with ISA devices, and we need
		// to look them up in the PRT.
		pcihb_PRT.Length = ACPI_ALLOCATE_BUFFER;
		if (ACPI_STATUS status = AcpiGetIrqRoutingTable(acpiHandle, &pcihb_PRT); ACPI_FAILURE(status)) {
			Printf("could not get PCI interrupt routing table: %s", AcpiFormatException(status));
		}

		return Result::Success();
	}

	Result Detach() override
	{
		return Result::Success();
	}

	Result AllocateIRQ(Device& device, int index, irq::IHandler& handler) override
	{
		auto res_devno = device.d_ResourceSet.GetResource(Resource::RT_PCI_Device, 0);
		KASSERT(res_devno != nullptr, "here without pci device?");
		uint32_t deviceNum = res_devno->r_Base;

		int interrupt = 0;
		interrupt = LookupPCIInterrupt(deviceNum, index);
		if (interrupt < 0)
			return RESULT_MAKE_FAILURE(ENODEV);

		if (interrupt == 0)
		{
			auto res_irqno = device.d_ResourceSet.GetResource(Resource::RT_IRQ, 0);
			if (res_irqno == nullptr)
				return RESULT_MAKE_FAILURE(ENODEV);
			interrupt = res_irqno->r_Base;
		}

		if (interrupt == 18)
			interrupt++;

		Printf("assigning IRQ %d for device %s%d", interrupt, device.d_Name, device.d_Unit);
		return irq::Register(interrupt, &device, IRQ_TYPE_DEFAULT, handler);
	}

	int LookupPCIInterrupt(int deviceNumber, int pin) const
	{
		int result = -1;
		WalkPCIResourceTable(pcihb_PRT, [&](const ACPI_PCI_ROUTING_TABLE& entry)
		{
			if (entry.Pin != pin)
				return false;
			if (ADRToDeviceNumber(entry.Address) != deviceNumber)
				return false;
			result = entry.SourceIndex;
			return true;
		});
		return result;
	}

private:
	ACPI_BUFFER pcihb_PRT{};
};

struct ACPI_PCIHB_Driver : public Driver
{
	ACPI_PCIHB_Driver()
	 : Driver("acpi-pcihb")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return "acpi";
	}

	Device* CreateDevice(const CreateDeviceProperties& cdp) override
	{
		auto res = cdp.cdp_ResourceSet.GetResource(Resource::RT_PNP_ID, 0);
		if (res != NULL && res->r_Base == 0x0a03 /* PNP0A03: PCI Bus */)
			return new ACPI_PCIHB(cdp);
		return nullptr;
	}
};

const RegisterDriver<ACPI_PCIHB_Driver> registerDriver;

} // namespace

/* vim:set ts=2 sw=2: */
