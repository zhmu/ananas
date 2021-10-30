/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2019 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/util/atomic.h>

/*
 * Spinlocks are simply locks that just keep the CPU busy waiting if they can't
 * be acquired. They can be used in any context and are designed to be light-
 * weight. They come in a normal and preemptible flavor; the latter will disable
 * interrupts. XXX It's open to debate whether this should always be the case
 */
class Spinlock final
{
  public:
    Spinlock();
    Spinlock(const Spinlock&) = delete;
    Spinlock& operator=(const Spinlock&) = delete;

    // Ordinary spinlocks which can be preempted at any time */
    void Lock();
    void Unlock();

    // Unpremptible spinlocks disable the interrupt flag while they are active
    register_t LockUnpremptible();
    void UnlockUnpremptible(register_t state);

    void AssertLocked();
    void AssertUnlocked();

  private:
    util::atomic<int> sl_var;
};
