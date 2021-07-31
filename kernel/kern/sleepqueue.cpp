/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2019 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/pcpu.h"
#include "kernel/sleepqueue.h"
#include "kernel/thread.h"
#include "kernel-md/interrupts.h"

namespace
{
    void WakeupWaiter(sleep_queue::Waiter& waiter)
    {
        auto& curThread = thread::GetCurrent();
        auto& waitingThread = *waiter.w_thread;

        waiter.w_signalled = true;
        waitingThread.Resume();

        if (curThread.t_priority < waitingThread.t_priority)
            curThread.t_flags |= THREAD_FLAG_RESCHEDULE;
    }
} // unnamed namespace

SleepQueue::SleepQueue(const char* name) : sq_name(name) {}

sleep_queue::Waiter* SleepQueue::DequeueWaiter()
{
    SpinlockUnpremptibleGuard g(sq_lock);
    if (sq_sleepers.empty())
        return NULL;

    auto& t = sq_sleepers.front();
    sq_sleepers.pop_front();

    return &t.t_sqwaiter;
}

bool SleepQueue::WakeupOne()
{
    auto waiter = DequeueWaiter();
    if (!waiter) return false;

    WakeupWaiter(*waiter);
    return true;
}

void SleepQueue::WakeupAll()
{
    while(WakeupOne())
        ;
}

sleep_queue::Sleeper SleepQueue::PrepareToSleep()
{
    auto& curThread = thread::GetCurrent();

    auto state = sq_lock.LockUnpremptible();
    KASSERT(
        curThread.t_sqwaiter.w_sq == nullptr, "sleeping thread %p that is already sleeping",
        &curThread);
    curThread.t_sqwaiter = sleep_queue::Waiter{&curThread, this};

    // XXX Maybe we ought to keep the sq_sleepers list sorted?
    sq_sleepers.push_back(curThread);

    return sleep_queue::Sleeper{*this, state};
}

void SleepQueue::Sleep(register_t state)
{
    auto& curThread = thread::GetCurrent();
    auto& sqwaiter = curThread.t_sqwaiter;

    sq_lock.AssertLocked();
    KASSERT(!sq_sleepers.empty(), "attempting sleep without preparing?");
    while (true) {
        KASSERT(sqwaiter.w_sq == this, "corrupt waiter");
        if (sqwaiter.w_signalled) {
            // We woke up - reset the waiter
            sqwaiter = sleep_queue::Waiter();
            sq_lock.UnlockUnpremptible(state); // restores interrupts
            return;
        }
        curThread.Suspend();
        sq_lock.Unlock(); // note: keeps interrupts disabled
        scheduler::Schedule();

        // We are back, awoken by WakeupWaiter()
        sq_lock.Lock();
    }

    // NOTREACHED
}

void SleepQueue::Unsleep(register_t state)
{
    auto& curThread = thread::GetCurrent();
    auto& sqwaiter = curThread.t_sqwaiter;

    sq_lock.AssertLocked();
    for (auto& t : sq_sleepers) {
        if (&t != &curThread)
            continue;
        sq_sleepers.remove(t);
        if (sqwaiter.w_signalled) {
            // XXX not sure if this can happen, but no chances for now...
            panic("about to remove already signalled waiter");
        }
        sqwaiter = sleep_queue::Waiter();
        sq_lock.UnlockUnpremptible(state); // restores interrupts
        return;
    }

    panic("unsleep called but caller did not prepare");
}

struct sleep_queue::KDB
{
    KDB(SleepQueue& sq)
    {
        kprintf("name '%s'\n", sq.GetName());
        for(auto& t: sq.sq_sleepers) {
            auto& w = t.t_sqwaiter;
            kprintf("   sleeper thread %p: w_thread %p w_sq %p signalled %d\n", &t, w.w_thread, w.w_sq, w.w_signalled);
        }
    }
};

const kdb::RegisterCommand
    kdbScheduler("sleepq", "i:pointer", "Display sleepqueue status", [](int, const kdb::Argument* arg) {
        SleepQueue& sq = *reinterpret_cast<SleepQueue*>(arg[1].a_u.u_value);
        sleep_queue::KDB kdb{sq};
    });
