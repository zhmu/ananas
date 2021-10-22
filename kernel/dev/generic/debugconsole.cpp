/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/debug-console.h"
#include "kernel/device.h"
#include "kernel/init.h"
#include "kernel/mm.h"
#include "kernel/lib.h"
#include "kernel/lock.h"
#include "kernel/result.h"

namespace
{
    class DebugConsole : public Device, IDeviceOperations, ICharDeviceOperations
    {
      public:
        DebugConsole()
        {
            d_Major = device_manager::AllocateMajor();
            d_Unit = 0;
            strcpy(d_Name, "debugcon");
        }
        virtual ~DebugConsole() = default;

        IDeviceOperations& GetDeviceOperations() override { return *this; }
        ICharDeviceOperations* GetCharDeviceOperations() override { return this; }

        Result Attach() override { return Result::Success(); }
        Result Detach() override { return Result::Success(); }

        Result Read(VFS_FILE& file, void* buf, size_t len) override
        {
            return Result::Success(0);
        }

        Result Write(VFS_FILE& file, const void* buffer, size_t len) override
        {
            auto p = static_cast<const char*>(buffer);
            for(size_t n = 0; n < len; ++n) {
                debugcon_putch(*p);
                ++p;
            }
            return Result::Success(len);
        }
    };

    const init::OnInit init(init::SubSystem::Device, init::Order::Last, []() {
        device_manager::AttachSingle(*new DebugConsole);
    });
}
