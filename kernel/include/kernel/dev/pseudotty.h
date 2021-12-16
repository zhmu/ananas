/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/util/ring_buffer.h>
#include "kernel/condvar.h"
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
        util::ring_buffer<char, InputQueueSize> input_queue;
        Mutex mutex{"tty"};
        ConditionVariable cv_reader{"ttyread"};
        ConditionVariable cv_writer{"ttywrite"};
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
