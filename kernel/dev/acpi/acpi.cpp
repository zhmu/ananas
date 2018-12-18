#include "acpica/acpi.h"
#include "acpi.h"
#include "acpi_resource.h"
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/result.h"
#include "kernel/shutdown.h"
#include "kernel/trace.h"
#include "kernel-md/acpi.h"

TRACE_SETUP;

namespace
{
    struct ACPI : public Device, private IDeviceOperations {
        using Device::Device;

        static ACPI_STATUS
        AttachDevice(ACPI_HANDLE ObjHandle, UINT32 Level, void* Context, void** ReturnValue);
        IDeviceOperations& GetDeviceOperations() override { return *this; }

        Result Attach() override;
        Result Detach() override;

        void OnPowerButtonEvent();

        static UINT32 PowerButtonEventWrapper(void* context)
        {
            reinterpret_cast<ACPI*>(context)->OnPowerButtonEvent();
            return ACPI_INTERRUPT_HANDLED;
        }
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
        ResourceSet resourceSet;
        if (ACPI_SUCCESS(acpi_process_resources(ObjHandle, resourceSet))) {
            /* ... and see if we can attach something to these resources */
            device_manager::AttachChild(acpi, resourceSet);
        }

        return AE_OK;
    }

    Result ACPI::Attach()
    {
        ACPI_STATUS status;

        /* Initialize ACPI subsystem */
        status = AcpiInitializeSubsystem();
        if (ACPI_FAILURE(status))
            return RESULT_MAKE_FAILURE(ENODEV);

        /* Initialize tablde manager and fetch all tables */
        status = AcpiInitializeTables(NULL, 16, FALSE);
        if (ACPI_FAILURE(status))
            return RESULT_MAKE_FAILURE(ENODEV);

        /* Create ACPI namespace from ACPI tables */
        status = AcpiLoadTables();
        if (ACPI_FAILURE(status))
            return RESULT_MAKE_FAILURE(ENODEV);

        /* Enable ACPI hardware */
        status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
        if (ACPI_FAILURE(status))
            return RESULT_MAKE_FAILURE(ENODEV);

        /* Complete ACPI namespace object initialization */
        status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
        if (ACPI_FAILURE(status))
            return RESULT_MAKE_FAILURE(ENODEV);

        // Initialize power button callback
        if ((AcpiGbl_FADT.Flags & ACPI_FADT_POWER_BUTTON) == 0) {
            AcpiClearEvent(ACPI_EVENT_POWER_BUTTON);
            AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON, &PowerButtonEventWrapper, this);
        }

        // Inform ACPI of our interrupt model - we'll assume the presence of an APIC here
        {
            constexpr int pic_Mode_APIC = 1;
            ACPI_OBJECT obj;
            obj.Type = ACPI_TYPE_INTEGER;
            obj.Integer.Value = pic_Mode_APIC;

            ACPI_OBJECT_LIST objectList;
            objectList.Count = 1;
            objectList.Pointer = &obj;

            AcpiEvaluateObject(ACPI_ROOT_OBJECT, const_cast<char*>("_PIC"), &objectList, nullptr);
        }

        /*
         * Now enumerate through all ACPI devices and see what we can find.
         */
        AcpiWalkNamespace(
            ACPI_TYPE_ANY, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX, AttachDevice, NULL, this, NULL);

        //
        status = AcpiUpdateAllGpes();
        if (ACPI_FAILURE(status))
            Printf("could not update all GPEs: %s", AcpiFormatException(status));

        return Result::Success();
    }

    Result ACPI::Detach() { return Result::Success(); }

    void ACPI::OnPowerButtonEvent()
    {
        Printf("power button pressed");
        shutdown::RequestShutdown(shutdown::ShutdownType::PowerDown);
    }

    class ACPIDriver : public Driver
    {
      public:
        ACPIDriver();
        virtual ~ACPIDriver() = default;

        const char* GetBussesToProbeOn() const override;
        Device* CreateDevice(const CreateDeviceProperties& cdp) override;
    };

    ACPIDriver::ACPIDriver() : Driver("acpi") {}

    const char* ACPIDriver::GetBussesToProbeOn() const { return "corebus"; }

    Device* ACPIDriver::CreateDevice(const CreateDeviceProperties& cdp)
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

    const RegisterDriver<ACPIDriver> registerDriver;

} // unnamed namespace

void acpi_init()
{
    /*
     * Preform early initialization; this is used by the SMP code because it
     * needs the MADT table before the ACPI is fully initialized.
     */
    AcpiInitializeTables(NULL, 2, TRUE);
}

/* vim:set ts=2 sw=2: */
