/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/dev/pseudotty.h"
#include "kernel/driver.h"
#include "kernel/process.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/dev/tty.h"
#include <sys/tty.h>

namespace pseudotty
{
    namespace
    {
        constexpr inline auto debugPTTY = false;
    }

    Master::Master(const CreateDeviceProperties& cdp)
        : Device(cdp)
    {
    }

    Result Master::Attach()
    {
        return Result::Success();
    }

    Result Master::Detach()
    {
        return Result::Success();
    }

    Result Master::Read(VFS_FILE& file, void* buf, size_t len)
    {
        if (debugPTTY) kprintf("Master::read() len %d\n", len);
        auto data = static_cast<char*>(buf);
        while (1) {
            if (in_readpos == in_writepos) {
                /*
                 * Buffer is empty - schedule the thread for a wakeup once we have data.
                 */
                waiters.Wait();
                continue;
            }

            // Copy the data over
            int num_read = 0;
            while (len > 0) {
                if (in_readpos == in_writepos) break;
                data[num_read] = input_queue[in_readpos];
                if (debugPTTY) kprintf("Master::Read(): [%c]\n", data[num_read]);
                in_readpos = (in_readpos + 1) % input_queue.size();
                ++num_read;
                --len;
            }
            if (debugPTTY) kprintf("Master::Read(): done, num_read %d\n", num_read);
            return Result::Success(num_read);
        }
    }

    bool Master::CanRead()
    {
        return in_readpos != in_writepos;
    }

    Result Master::OnInput(const char* buffer, size_t len)
    {
        if (debugPTTY) kprintf("Master::OnInput(): len=%d\n", len);
        for (/* nothing */; len > 0; buffer++, len--) {
            char ch = *buffer;
            if (debugPTTY) kprintf("Master::OnInput(): [%c]\n", ch);

            if ((in_writepos + 1) % input_queue.size() == in_readpos)
                return Result::Failure(ENOSPC);

            // Store the charachter
            input_queue[in_writepos] = ch;
            in_writepos = (in_writepos + 1) % input_queue.size();

            // ???
            waiters.Signal();
        }

        if (debugPTTY) kprintf("Master::OnInput(): done\n");
        return Result::Success();
    }

    Result Master::Write(VFS_FILE& file, const void* buffer, size_t len)
    {
        KASSERT(pseudoTTY->p_slave != nullptr, "no slave?");
        return pseudoTTY->p_slave->OnInput(static_cast<const char*>(buffer), len);
    }

    Result Master::IOControl(Process* proc, unsigned long req, void* buffer[])
    {
        switch (req) {
            case TIOGDNAME: { // Get device name
                return pseudoTTY->p_slave->IOControl(proc, req, buffer);
            }
        }
        return Result::Failure(EINVAL);
    }

    Slave::Slave(const CreateDeviceProperties& cdp)
        : TTY(cdp)
    {
    }

    Result Slave::Attach()
    {
        return Result::Success();
    }

    Result Slave::Detach()
    {
        return Result::Success();
    }

    Result Slave::Print(const char* buffer, size_t len)
    {
        KASSERT(pseudoTTY->p_master != nullptr, "no master?");
        return pseudoTTY->p_master->OnInput(buffer, len);
    }

} // unnamed namespace

namespace pseudotty
{
    PseudoTTY* AllocatePseudoTTY()
    {
        CreateDeviceProperties cdp(ResourceSet{});
        auto master = new Master(cdp);
        strcpy(master->d_Name, "mtty");

        auto slave = static_cast<Slave*>(
            device_manager::CreateDevice("ptty", CreateDeviceProperties(cdp)));
        if (auto result = device_manager::AttachSingle(*slave); result.IsFailure())
            panic("unable to register tty slave: %d",  result.AsStatusCode());

        auto pseudoTTY = new PseudoTTY;
        pseudoTTY->p_master = master;
        pseudoTTY->p_slave = slave;
        master->pseudoTTY = pseudoTTY;
        slave->pseudoTTY = pseudoTTY;
        return pseudoTTY;
    }

    namespace
    {
        struct Slave_Driver : Driver {
            Slave_Driver() : Driver("ptty") { }

            const char* GetBussesToProbeOn() const override {
                // We are created directly by AllocatePseudoTTY
                return nullptr;
            }

            Device* CreateDevice(const CreateDeviceProperties& cdp) override
            {
                return new Slave(cdp);
            }
        };
        const RegisterDriver<Slave_Driver> registerDriver;
    }
}
