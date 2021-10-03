/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "kernel/device.h"
#include "kernel/dev/tty.h"

namespace pseudotty {
    struct Master;
    struct Slave;

    struct PseudoTTY
    {
        Master* p_master{};
        Slave* p_slave{};
    };

    struct Master : Device, private IDeviceOperations, private ICharDeviceOperations
    {
        PseudoTTY* pseudoTTY{};

        Master(const CreateDeviceProperties&);

        IDeviceOperations& GetDeviceOperations() override { return *this; }
        ICharDeviceOperations* GetCharDeviceOperations() override { return this; }

        Result Attach() override;
        Result Detach() override;
        Result IOControl(Process* proc, unsigned long req, void* buffer[]) override;

        Result OnInput(const char* buffer, size_t len);

        Result Read(VFS_FILE& file, void* buf, size_t len) override;
        Result Write(VFS_FILE& file, const void* buffer, size_t len) override;
        bool CanRead() override;

        static constexpr size_t InputQueueSize = 256;
        util::array<char, InputQueueSize> input_queue;
        unsigned int in_writepos = 0;
        unsigned int in_readpos = 0;
        Semaphore waiters{"tty", 1};
    };

    struct Slave : TTY
    {
        PseudoTTY* pseudoTTY{};

        Slave(const CreateDeviceProperties&);

        Result Attach() override;
        Result Detach() override;

        Result Print(const char* buffer, size_t len) override;
    };

    PseudoTTY* AllocatePseudoTTY();
}
