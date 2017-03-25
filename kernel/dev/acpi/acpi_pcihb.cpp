#include <ananas/device.h>
#include <ananas/driver.h>
#include <ananas/error.h>
#include "acpica/acpi.h"
#include "acpi.h"
#include "acpi_resource.h"

namespace {

struct ACPI_PCIHB : public Ananas::Device, private Ananas::IDeviceOperations
{
	using Device::Device;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	errorcode_t Attach() override
	{
		return ananas_success();
	}

	errorcode_t Detach() override
	{
		return ananas_success();
	}
};

struct ACPI_PCIHB_Driver : public Ananas::Driver
{
	ACPI_PCIHB_Driver()
	 : Driver("acpi-pcihb")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return "acpi";
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		auto res = cdp.cdp_ResourceSet.GetResource(Ananas::Resource::RT_PNP_ID, 0);
		if (res != NULL && res->r_Base == 0x0a03 /* PNP0A03: PCI Bus */)
			return new ACPI_PCIHB(cdp);
		return nullptr;
	}
};

} // namespace

REGISTER_DRIVER(ACPI_PCIHB_Driver)

/* vim:set ts=2 sw=2: */
