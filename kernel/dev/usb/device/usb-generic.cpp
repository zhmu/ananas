#include <ananas/types.h>
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "../core/config.h"
#include "../core/usb-core.h"
#include "../core/usb-device.h"

namespace
{
    class USBGeneric : public Device, private IDeviceOperations
    {
      public:
        using Device::Device;
        virtual ~USBGeneric() = default;

        IDeviceOperations& GetDeviceOperations() override { return *this; }

        Result Attach() override;
        Result Detach() override;

      private:
        usb::USBDevice* ug_Device;
    };

    Result USBGeneric::Attach()
    {
        ug_Device = static_cast<usb::USBDevice*>(
            d_ResourceSet.AllocateResource(Resource::RT_USB_Device, 0));
        return Result::Success();
    }

    Result USBGeneric::Detach() { return Result::Success(); }

    struct USBGeneric_Driver : public Driver {
        USBGeneric_Driver() : Driver("usbgeneric", 1000000)
        {
            // Note that we are a fall-back driver (we'll attach to anything USB), so
            // we have a very high priority value to push us to the end of the driver
            // list
        }

        const char* GetBussesToProbeOn() const override { return "usbbus"; }

        Device* CreateDevice(const CreateDeviceProperties& cdp) override
        {
            // We accept any USB device
            auto res = cdp.cdp_ResourceSet.GetResource(Resource::RT_USB_Device, 0);
            if (res == nullptr)
                return nullptr;

            return new USBGeneric(cdp);
        }
    };

    const RegisterDriver<USBGeneric_Driver> registerDriver;

} // unnamed namespace

/* vim:set ts=2 sw=2: */
