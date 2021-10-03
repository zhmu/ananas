/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/device.h"
#include "kernel/init.h"
#include "kernel/lib.h"
#include "kernel/result.h"

namespace
{
    class Null : public Device, private IDeviceOperations, private ICharDeviceOperations
    {
      public:
        Null()
        {
            d_Major = device_manager::AllocateMajor();
            d_Unit = 0;
            strcpy(d_Name, "null");
        }
        virtual ~Null() = default;

        IDeviceOperations& GetDeviceOperations() override { return *this; }
        ICharDeviceOperations* GetCharDeviceOperations() override { return this; }

        Result Attach() override { return Result::Success(); }
        Result Detach() override { return Result::Success(); }

        Result Read(VFS_FILE& file, void* buf, size_t len) override { return Result::Success(0); }

        Result Write(VFS_FILE& file, const void* buffer, size_t len) override
        {
            return Result::Success(len);
        }
    };

    const init::OnInit attachCoreBus(init::SubSystem::Device, init::Order::Last, []() {
        device_manager::AttachSingle(*new Null);
    });

} // unnamed namespace
