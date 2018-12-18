/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/lock.h"
#include "kernel/lib.h"
#include "kernel/dev/kbdmux.h"

namespace keyboard_mux
{
    namespace
    {
        Mutex mutex{"kbdmutex"};

        util::List<IKeyboardConsumer> consumers;

    } // unnamed namespace

    void RegisterConsumer(IKeyboardConsumer& consumer)
    {
        MutexGuard g(mutex);
        consumers.push_back(consumer);
    }

    void UnregisterConsumer(IKeyboardConsumer& consumer)
    {
        MutexGuard g(mutex);
        consumers.remove(consumer);
    }

    void OnKey(const Key& key, int modifiers)
    {
        MutexGuard g(mutex);
        for (auto& consumer : consumers)
            consumer.OnKey(key, modifiers);
    }

} // namespace keyboard_mux
