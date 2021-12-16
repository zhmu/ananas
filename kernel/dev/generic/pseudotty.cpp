/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/flags.h>
#include "kernel/dev/pseudotty.h"
#include "kernel/driver.h"
#include "kernel/process.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/dev/tty.h"
#include "kernel/vfs/types.h"
#include <ananas/tty.h>

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
        mutex.Lock();
        while (1) {
            if (input_queue.empty()) {
                /*
                 * Buffer is empty - schedule the thread for a wakeup once we have data.
                 */
                if (file.f_flags & O_NONBLOCK) {
                    mutex.Unlock();
                    return Result::Failure(EAGAIN);
                }
                cv_reader.Wait(mutex);
                continue;
            }

            // Copy the data over
            int num_read = 0;
            while (len > 0) {
                if (input_queue.empty()) break;
                data[num_read] = input_queue.front();
                if (debugPTTY) kprintf("Master::Read(): [%c]\n", data[num_read]);
                input_queue.pop_front();
                ++num_read;
                --len;
            }
            if (debugPTTY) kprintf("Master::Read(): done, num_read %d\n", num_read);
            cv_writer.Signal(); // bytes have been consumed
            mutex.Unlock();
            return Result::Success(num_read);
        }
    }

    bool Master::CanRead()
    {
        return !input_queue.empty();
    }

    Result Master::OnInput(const char* buffer, size_t len)
    {
        if (debugPTTY) kprintf("Master::OnInput(): len=%d\n", len);
        mutex.Lock();
        for (/* nothing */; len > 0; buffer++, len--) {
            char ch = *buffer;
            if (debugPTTY) kprintf("Master::OnInput(): [%c]\n", ch);

            if (input_queue.full()) {
                // TODO how to handle nowait?!
                if (debugPTTY) kprintf("Master::OnInput(): queue is full\n");
                cv_writer.Wait(mutex);
                continue;
            }

            // Store the charachter and trigger any readers
            input_queue.push_back(ch);
            cv_reader.Signal();
        }
        mutex.Unlock();

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
