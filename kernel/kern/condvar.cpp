/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2019 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/condvar.h"
#include "kernel/lock.h"

ConditionVariable::ConditionVariable(const char* name) : Lockable(name) {}

void ConditionVariable::Signal() {
    DisableInterruptGuard ig;
    cv_sleepq.WakeupOne(ig);
}

void ConditionVariable::Broadcast() {
    DisableInterruptGuard ig;
    cv_sleepq.WakeupAll(ig);
}

void ConditionVariable::Wait(Mutex& mutex)
{
    mutex.AssertLocked();

    auto sleep = cv_sleepq.PrepareToSleep(*this);
    mutex.Unlock();
    sleep.Sleep();
    mutex.Lock();
}
