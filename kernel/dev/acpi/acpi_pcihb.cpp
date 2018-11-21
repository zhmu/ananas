#include "acpica/acpi.h"
#include "acpi.h"
#include "acpi_resource.h"
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/result.h"

namespace {

struct ACPI_PCIHB : public Device, private IDeviceOperations
{
	using Device::Device;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	Result Attach() override
	{
		return Result::Success();
	}

	Result Detach() override
	{
		return Result::Success();
	}
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
