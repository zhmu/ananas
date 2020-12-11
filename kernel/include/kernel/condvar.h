/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2019 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "kernel/sleepqueue.h"

struct Mutex;

struct ConditionVariable {
    ConditionVariable(const char* name);

    void Signal();
    void Broadcast();

    void Wait(Mutex& mutex);

private:
    SleepQueue cv_sleepq;
};
