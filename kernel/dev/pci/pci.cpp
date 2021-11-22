/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/dev/pci.h"
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/result.h"
#include "kernel/lib.h"
#include "kernel-md/io.h"
#include "kernel-md/pcihb.h"

namespace
{
    class PCI : public Device, private IDeviceOperations
    {
      public:
        using Device::Device;
        virtual ~PCI() = default;

        IDeviceOperations& GetDeviceOperations() override { return *this; }

        Result Attach() override;
        Result Detach() override;
    };

    Result PCI::Attach()
    {
        for (unsigned int bus = 0; bus < PCI_MAX_BUSSES; bus++) {
            /*
             * Attempt to obtain the vendor/device ID of device 0 on this bus. If it
             * does not exist, we assume the bus doesn't exist.
             */
            const pci::Identifier pciBusId{ bus, 0, 0 };
            const auto dev_vendor = pci::ReadConfig<uint32_t>(pciBusId, PCI_REG_DEVICEVENDOR);
            if ((dev_vendor & 0xffff) == PCI_NOVENDOR)
                continue;

            ResourceSet resourceSet;
            resourceSet.AddResource(Resource(Resource::RT_PCI_Bus, bus, 0));
            resourceSet.AddResource(Resource(Resource::RT_PCI_VendorID, dev_vendor & 0xffff, 0));
            resourceSet.AddResource(Resource(Resource::RT_PCI_DeviceID, dev_vendor >> 16, 0));

            Device* new_bus =
                device_manager::CreateDevice("pcibus", CreateDeviceProperties(*this, resourceSet));
            if (new_bus != nullptr)
                device_manager::AttachSingle(*new_bus);
        }
        return Result::Success();
    }

    Result PCI::Detach() { return Result::Success(); }

    struct PCI_Driver : public Driver {
        PCI_Driver() : Driver("pci") {}

        const char* GetBussesToProbeOn() const override { return "pcihb,acpi-pcihb"; }

        Device* CreateDevice(const CreateDeviceProperties& cdp) override { return new PCI(cdp); }
    };

    const RegisterDriver<PCI_Driver> registerDriver;
} // unnamed namespace

namespace pci {
    namespace detail {
        Identifier MakePCIIdentifier(const Device& device)
        {
            const auto& resourceSet = device.d_ResourceSet;
            const auto bus_res = resourceSet.GetResource(Resource::RT_PCI_Bus, 0);
            const auto dev_res = resourceSet.GetResource(Resource::RT_PCI_Device, 0);
            const auto func_res = resourceSet.GetResource(Resource::RT_PCI_Function, 0);
            KASSERT(bus_res && dev_res && func_res, "missing pci resources");
            return {
                static_cast<pci::BusNo>(bus_res->r_Base),
                static_cast<pci::DeviceNo>(dev_res->r_Base),
                static_cast<pci::FuncNo>(func_res->r_Base)
            };
        }
    }

    void EnableBusmaster(Device& device, bool on)
    {
        auto cmd = ReadConfig<uint32_t>(device, PCI_REG_STATUSCOMMAND);
        if (on)
            cmd |= PCI_CMD_BM;
        else
            cmd &= ~PCI_CMD_BM;
        WriteConfig(device, PCI_REG_STATUSCOMMAND, cmd);
    }
}
