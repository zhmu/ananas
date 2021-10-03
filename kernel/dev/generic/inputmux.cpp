/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/lock.h"
#include "kernel/condvar.h"
#include "kernel/lib.h"
#include "kernel/device.h"
#include "kernel/dev/inputmux.h"
#include "kernel/init.h"
#include <ananas/util/array.h>
#include <ananas/inputmux.h>

namespace input_mux
{
    namespace
    {
        Mutex mutex{"inputmux"};

        util::List<IInputConsumer> consumers;

    } // unnamed namespace

    void RegisterConsumer(IInputConsumer& consumer)
    {
        MutexGuard g(mutex);
        consumers.push_back(consumer);
    }

    void UnregisterConsumer(IInputConsumer& consumer)
    {
        MutexGuard g(mutex);
        consumers.remove(consumer);
    }

    void OnEvent(const AIMX_EVENT& event)
    {
        MutexGuard g(mutex);
        for (auto& consumer : consumers)
            consumer.OnEvent(event);
    }
} // namespace input_mux
