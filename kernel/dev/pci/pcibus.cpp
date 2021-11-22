/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/dev/pci.h"
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel-md/pcihb.h"

namespace
{
    class PCIBus : public Device, private IDeviceOperations
    {
      public:
        using Device::Device;
        virtual ~PCIBus() = default;

        IDeviceOperations& GetDeviceOperations() override { return *this; }

        Result Attach() override;
        Result Detach() override;
    };

    Result PCIBus::Attach()
    {
        auto busResource = d_ResourceSet.GetResource(Resource::RT_PCI_Bus, 0);
        KASSERT(busResource != nullptr, "called without a PCI bus resource");

        unsigned int busno = busResource->r_Base;
        for (unsigned int devno = 0; devno < PCI_MAX_DEVICES; devno++) {
            const pci::Identifier pciId{ busno, devno, 0 };
            auto dev_vendor = pci::ReadConfig<uint32_t>(pciId, PCI_REG_DEVICEVENDOR);
            if ((uint32_t)(dev_vendor & 0xffff) == PCI_NOVENDOR)
                continue; /* nothing here */

            /*
             * Fetch the header-type; this is used to figure out whether this is an
             * ordinary PCI device or not
             */
            unsigned int max_func = 1;
            if (pci::ReadConfig<uint32_t>(pciId, PCI_REG_HEADERTIMER) & PCI_HEADER_MULTI)
                max_func = PCI_MAX_FUNCS;

            for (unsigned int funcno = 0; funcno < max_func; funcno++) {
                const pci::Identifier pciFuncId{ busno, devno, funcno };
                dev_vendor = pci::ReadConfig<uint32_t>(pciFuncId, PCI_REG_DEVICEVENDOR);
                if ((uint32_t)(dev_vendor & 0xffff) == PCI_NOVENDOR)
                    continue; /* nothing here */

                /*
                 * Retrieve the PCI device class; drivers may use this to determine whether
                 * they need to attach.
                 */
                const auto class_revision =
                    pci::ReadConfig<uint32_t>(pciFuncId, PCI_REG_CLASSREVISION);

                // Collect the PCI resources
                ResourceSet resourceSet;
                resourceSet.AddResource(Resource(Resource::RT_PCI_Bus, busno, 0));
                resourceSet.AddResource(Resource(Resource::RT_PCI_Device, devno, 0));
                resourceSet.AddResource(Resource(Resource::RT_PCI_Function, funcno, 0));
                resourceSet.AddResource(
                    Resource(Resource::RT_PCI_VendorID, dev_vendor & 0xffff, 0));
                resourceSet.AddResource(Resource(Resource::RT_PCI_DeviceID, dev_vendor >> 16, 0));
                resourceSet.AddResource(Resource(Resource::RT_PCI_ClassRev, class_revision, 0));

                /* Walk through the BAR registers */
                for (unsigned int bar = PCI_REG_BAR0; bar <= PCI_REG_BAR5; bar += 4) {
                    const auto res = pci::ReadConfig<uint32_t>(pciFuncId, bar);
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
                    pci::WriteConfig<uint32_t>(pciFuncId, bar, 0xffffffff);
                    auto len = pci::ReadConfig<uint32_t>(pciFuncId, bar);
                    pci::WriteConfig<uint32_t>(pciFuncId, bar, res);
                    if (len == 0 || len == 0xffffffff)
                        continue;

                    /* This appears to be a real resource; need to add it */
                    if (res & PCI_BAR_MMIO) {
                        /* memory mapped I/O space */
                        uint32_t mmio = res & 0xfffffffc;
                        len = 0xfffffffc & len;
                        if (mmio != 0 && len != 0) {
                            len = (len & ~(len - 1)) - 1;
                            resourceSet.AddResource(Resource(Resource::RT_IO, mmio, len));
                        }
                    } else {
                        /* memory space */
                        addr_t mem = res & 0xfffffff0;
                        len = 0xfffffff0 & len;
                        if (mem != 0 && len != 0) {
                            len = (len & ~(len - 1)) - 1;
                            resourceSet.AddResource(Resource(Resource::RT_Memory, mem, len));
                        }
                    }
                }

                /* Fetch the IRQ line, if any */
                const auto irq = pci::ReadConfig<uint32_t>(pciFuncId, PCI_REG_INTERRUPT) & 0xff;
                if (irq != 0 && irq != 0xff) {
                    resourceSet.AddResource(Resource(Resource::RT_IRQ, irq, 0));
                }

                /* Attempt to attach this new child device */
                if (device_manager::AttachChild(*this, resourceSet) != nullptr)
                    continue;
#if 0
			Printf("no match for vendor 0x%x device 0x%x class %u, device ignored",
				dev_vendor & 0xffff, dev_vendor >> 16, PCI_CLASS(class_revision));
#endif
            }
        }
        return Result::Success();
    }

    Result PCIBus::Detach() { return Result::Success(); }

    struct PCIBus_Driver : public Driver {
        PCIBus_Driver() : Driver("pcibus") {}

        const char* GetBussesToProbeOn() const override { return nullptr; }

        Device* CreateDevice(const CreateDeviceProperties& cdp) override { return new PCIBus(cdp); }
    };

    const RegisterDriver<PCIBus_Driver> registerDriver;

} // unnamed namespace
