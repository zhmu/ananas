#include <ananas/error.h>
#include "acpica/acpi.h"
#include "acpi.h"
#include "acpi_resource.h"
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/trace.h"
#include "kernel/x86/acpi.h"

TRACE_SETUP;

namespace {

struct ACPI : public Ananas::Device, private Ananas::IDeviceOperations
{
	using Device::Device;

	static ACPI_STATUS AttachDevice(ACPI_HANDLE ObjHandle, UINT32 Level, void* Context, void** ReturnValue);
	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	errorcode_t Attach() override;
	errorcode_t Detach() override;
};

ACPI_STATUS
ACPI::AttachDevice(ACPI_HANDLE ObjHandle, UINT32 Level, void* Context, void** ReturnValue)
{
	ACPI_OBJECT_TYPE type;
	if (ACPI_FAILURE(AcpiGetType(ObjHandle, &type)))
		return AE_OK;

	/* Grab the device name; this is handy for debugging */
	char name[256];
	ACPI_BUFFER buf;
	buf.Length = sizeof(name);
	buf.Pointer = name;
	if (ACPI_FAILURE(AcpiGetName(ObjHandle, ACPI_FULL_PATHNAME, &buf)))
		return AE_OK;

	auto& acpi = *static_cast<ACPI*>(Context);

	/* Fetch the device resources... */
	Ananas::ResourceSet resourceSet;
	if (ACPI_SUCCESS(acpi_process_resources(ObjHandle, resourceSet))) {
		/* ... and see if we can attach something to these resources */
		Ananas::DeviceManager::AttachChild(acpi, resourceSet);
	}

	return AE_OK;
}

errorcode_t
ACPI::Attach()
{
	ACPI_STATUS status;

	/* Initialize ACPI subsystem */
	status = AcpiInitializeSubsystem();
	if (ACPI_FAILURE(status))
		return ANANAS_ERROR(NO_DEVICE);

	/* Initialize tablde manager and fetch all tables */
	status = AcpiInitializeTables(NULL, 16, FALSE);
	if (ACPI_FAILURE(status))
		return ANANAS_ERROR(NO_DEVICE);

	/* Create ACPI namespace from ACPI tables */
	status = AcpiLoadTables();
	if (ACPI_FAILURE(status))
		return ANANAS_ERROR(NO_DEVICE);

	/* Enable ACPI hardware */
	status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
	if (ACPI_FAILURE(status))
		return ANANAS_ERROR(NO_DEVICE);

	/* Complete ACPI namespace object initialization */
	status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
	if (ACPI_FAILURE(status))
		return ANANAS_ERROR(NO_DEVICE);

	/*
	 * Now enumerate through all ACPI devices and see what we can find.
	 */
	AcpiWalkNamespace(ACPI_TYPE_ANY, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX, AttachDevice, NULL, this, NULL);

	return ananas_success();
}

errorcode_t
ACPI::Detach()
{
	return ananas_success();
}

class ACPIDriver : public Ananas::Driver
{
public:
	ACPIDriver();
	virtual ~ACPIDriver() = default;

  const char* GetBussesToProbeOn() const override;
	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override;

};

ACPIDriver::ACPIDriver()
	: Driver("acpi")
{
}

const char* ACPIDriver::GetBussesToProbeOn() const
{
	return "corebus";
}

Ananas::Device* ACPIDriver::CreateDevice(const Ananas::CreateDeviceProperties& cdp)
{
	/*
	 * XXX This means we'll end up finding the root pointer twice if it exists
	 * since the attach function will do it too...
	 */
	ACPI_SIZE TableAddress;
	if (AcpiFindRootPointer(&TableAddress) != AE_OK)
		return NULL;
	return new ACPI(cdp);
}

REGISTER_DRIVER(ACPIDriver);

} // unnamed namespace

void
acpi_init()
{
	/*
	 * Preform early initialization; this is used by the SMP code because it
	 * needs the MADT table before the ACPI is fully initialized.
	 */
	AcpiInitializeTables(NULL, 2, TRUE);
}


/* vim:set ts=2 sw=2: */
