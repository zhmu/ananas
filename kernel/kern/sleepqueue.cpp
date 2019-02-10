/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2019 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/lib.h"
#include "kernel/pcpu.h"
#include "kernel/sleepqueue.h"
#include "kernel/thread.h"
#include "kernel-md/interrupts.h"

namespace
{
    // XXX We use a single lock for all sleepqueues on the system
    void WakeupWaiter(sleep_queue::Waiter& waiter)
    {
        Thread* curthread = PCPU_GET(curthread);

        waiter.w_signalled = true;
        waiter.w_thread->Resume();

        if (curthread->t_priority < waiter.w_thread->t_priority)
            curthread->t_flags |= THREAD_FLAG_RESCHEDULE;
    }

} // unnamed namespace

SleepQueue::SleepQueue(const char* name) : sq_name(name) {}

bool SleepQueue::WakeupOne()
{
    SpinlockUnpremptibleGuard g(sq_lock);
    if (sq_sleepers.empty())
        return false;

    // Fetch the first sleeper and remove it from the list
    auto& t = sq_sleepers.front();
    sq_sleepers.pop_front();

    // XXX we could drop the lock here
    WakeupWaiter(t.t_sqwaiter);
    return true;
}

void SleepQueue::WakeupAll()
{
    SpinlockUnpremptibleGuard g(sq_lock);

    while (!sq_sleepers.empty()) {
        auto& t = sq_sleepers.front();
        sq_sleepers.pop_front();

        WakeupWaiter(t.t_sqwaiter);
    }
}

sleep_queue::Sleeper SleepQueue::PrepareToSleep()
{
    Thread* curthread = PCPU_GET(curthread);

    auto state = sq_lock.LockUnpremptible();
    KASSERT(
        curthread->t_sqwaiter.w_sq == nullptr, "sleeping thread %p that is already sleeping",
        curthread);
    curthread->t_sqwaiter = sleep_queue::Waiter{curthread, this};

    // XXX Maybe we ought to keep the sq_sleepers list sorted?
    sq_sleepers.push_back(*curthread);

    return sleep_queue::Sleeper{*this, state};
}

void SleepQueue::Sleep(register_t state)
{
    Thread* curthread = PCPU_GET(curthread);
    auto& sqwaiter = curthread->t_sqwaiter;

    sq_lock.AssertLocked();
    KASSERT(HaveSleepers(), "attempting sleep without preparing?");
    while (true) {
        KASSERT(sqwaiter.w_sq == this, "corrupt waiter");
        if (sqwaiter.w_signalled) {
            // We woke up - reset the waiter
            sqwaiter = sleep_queue::Waiter();
            sq_lock.UnlockUnpremptible(state); // restores interrupts
            return;
        }
        curthread->Suspend();
        sq_lock.Unlock(); // note: keeps interrupts disabled
        scheduler::Schedule();

        // We are back, awoken by WakeupWaiter()
        sq_lock.Lock();
    }

    // NOTREACHED
}

void SleepQueue::Unsleep(register_t state)
{
    Thread* curthread = PCPU_GET(curthread);
    auto& sqwaiter = curthread->t_sqwaiter;

    sq_lock.AssertLocked();
    KASSERT(HaveSleepers(), "attempting unsleep without preparing?");
    for (auto& t : sq_sleepers) {
        if (&t != curthread)
            continue;
        sq_sleepers.remove(t);
        sqwaiter = sleep_queue::Waiter();
        sq_lock.UnlockUnpremptible(state); // restores interrupts
        return;
    }

    panic("unsleep called but caller did not prepare");
}

bool SleepQueue::HaveSleepers() const { return !sq_sleepers.empty(); }
