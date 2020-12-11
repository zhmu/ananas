/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>
#include <ananas/util/list.h>

namespace init
{
    enum class SubSystem {
        Handle = 0x0100000,
        KDB = 0x0110000,
        Driver = 0x0118000,
        Console = 0x0120000,
        // After this point, there is a console
        Process = 0x01f0000,
        Thread = 0x0200000,
        SMP = 0x0210000,
        TTY = 0x0220000,
        BIO = 0x0230000,
        Device = 0x0240000,
        VFS = 0x0250000,
        Scheduler = 0xf000000,
        Last = 0xfffffff // Do not use the last subsystem */
    };

    enum class Order {
        First = 0x0000001,
        Second = 0x0000002,
        Middle = 0x4000000,
        Any = 0x8000000,
        Last = 0xfffffff
    };

    struct OnInit;

    namespace internal
    {
        void Register(OnInit& onInit);
    }

    struct OnInit : util::List<OnInit>::NodePtr {
        template<typename Func>
        OnInit(init::SubSystem subsystem, init::Order order, Func fn)
            : oi_subsystem(subsystem), oi_order(order), oi_func(fn)
        {
            init::internal::Register(*this);
        }

        init::SubSystem oi_subsystem;
        init::Order oi_order;
        void (*oi_func)();
    };

} // namespace init

extern "C" void mi_startup();
