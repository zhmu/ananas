/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/irq.h"
#include "kernel/result.h"

namespace
{
    class CoreBus : public Device, private IDeviceOperations, private IBusOperations
    {
      public:
        CoreBus()
        {
            d_Major = device_manager::AllocateMajor();
            d_Unit = 0;
            strcpy(d_Name, "corebus");
        }

        IDeviceOperations& GetDeviceOperations() override { return *this; }

        IBusOperations& GetBusDeviceOperations() override { return *this; }

        Result Attach() override { return Result::Success(); }

        Result Detach() override { return Result::Success(); }

        Result AllocateIRQ(Device& device, int index, irq::IIRQCallback& callback) override
        {
            void* res_irq = device.d_ResourceSet.AllocateResource(Resource::RT_IRQ, 0);
            if (res_irq == NULL)
                return Result::Failure(ENODEV);

            return irq::RegisterIST((int)(uintptr_t)res_irq, &device, callback);
        }
    };

    const init::OnInit attachCoreBus(init::SubSystem::Device, init::Order::Last, []() {
        device_manager::AttachSingle(*new CoreBus);
    });

} // unnamed namespace
