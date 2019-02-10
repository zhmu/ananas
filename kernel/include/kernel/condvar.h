/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2019 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANAS_CONDITIONVARIABLE_H
#define ANANAS_CONDITIONVARIABLE_H

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

#endif // ANANAS_CONDITIONVARIABLE_H
