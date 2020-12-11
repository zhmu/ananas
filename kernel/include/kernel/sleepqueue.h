/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2019 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>
#include <ananas/util/list.h>
#include "kernel/spinlock.h"
#include "kernel/thread_fwd.h"

struct Mutex;
struct SleepQueue;
struct Thread;

namespace sleep_queue
{
    struct Waiter {
        Thread* w_thread = nullptr;
        SleepQueue* w_sq = nullptr;
        bool w_signalled = false;
    };

    struct KDB;
    struct Sleeper;

} // namespace sleep_queue

struct SleepQueue {
    friend struct sleep_queue::Sleeper;
    friend struct sleep_queue::KDB;

    SleepQueue(const char* name);
    bool WakeupOne();
    void WakeupAll();

    sleep_queue::Sleeper PrepareToSleep();

    const char* GetName() const { return sq_name; }

  private:
    const char* sq_name;

    Spinlock sq_lock;
    thread::SleepQueueThreadList sq_sleepers;

    void Unsleep(register_t state);
    void Sleep(register_t state);

    sleep_queue::Waiter* DequeueWaiter();
};

namespace sleep_queue
{
    struct Sleeper {
        Sleeper(SleepQueue& sq, register_t state) : sl_sq(sq), sl_state(state) {}

        void Sleep() { sl_sq.Sleep(sl_state); }

        void Cancel() { sl_sq.Unsleep(sl_state); }

      private:
        SleepQueue& sl_sq;
        register_t sl_state;
    };
} // namespace sleep_queue
