/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
/*
 * This contains the scheduler; it has two queues: a runqueue (containing all
 * threads that can run) and a sleepqueue (threads which cannot run). The
 * current thread is never on either of these queues; the reason is that the
 * administration of threads is distinct from the scheduler, and having the
 * scheduler re-add a thread that has expired its timeslice back to the
 * runqueue avoids nasty races (as well as being much easier to follow)
 */
#include "kernel/lock.h"
#include "kernel/init.h"
#include "kernel/lib.h"
#include "kernel/pcpu.h"
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/schedule.h"
#include "kernel/thread.h"
#include "kernel/time.h"
#include "kernel-md/interrupts.h"
#include "kernel-md/md.h"
#include "kernel-md/vm.h"

namespace scheduler
{
    namespace
    {
        // If set, ensure scheduler invariants hold. These really hurt performance and tend to hide
        // races, so best to keep them disabled by default
        constexpr bool isProveActive = false;
        bool isActive = false;

        using SchedLock = Spinlock;
        using SchedLockGuard = SpinlockUnpremptibleGuard;

        SchedLock schedLock;
        thread::SchedulerThreadList sched_runqueue;
        thread::SchedulerThreadList sched_sleepqueue;

        struct NotOnAnyQueue {
            constexpr static inline bool onRunQueue = false;
            constexpr static inline bool onSleepQueue = false;
        };
        struct OnlyOnRunQueue {
            constexpr static inline bool onRunQueue = true;
            constexpr static inline bool onSleepQueue = false;
        };
        struct OnlyOnSleepQueue {
            constexpr static inline bool onRunQueue = false;
            constexpr static inline bool onSleepQueue = true;
        };

        // Must be called with schedLock held
        template<typename What>
        void Prove(Thread& t)
        {
            if constexpr (!isProveActive)
                return;

            const auto onQueue = [](auto& q, auto& t) {
                for (auto& s : q) {
                    if (&s == &t)
                        return true;
                }
                return false;
            };

            const auto onRunQueue = onQueue(sched_runqueue, t);
            const auto onSleepQueue = onQueue(sched_sleepqueue, t);
            if (onRunQueue == What::onRunQueue && onSleepQueue == What::onSleepQueue)
                return;
            panic(
                "prove failed: expected thread %p onRunQueue %d, onSleepQueue %d but got onRunQueue %d "
                "onSleepQueue %d",
                &t,
                What::onRunQueue, What::onSleepQueue, onRunQueue, onSleepQueue);
        }

        bool IsActive(Thread& t) { return (t.t_sched_flags & THREAD_SCHED_ACTIVE) != 0; }
        bool IsSuspended(Thread& t) { return (t.t_sched_flags & THREAD_SCHED_SUSPENDED) != 0; }

        // Must be called with schedLock held
        void AddThreadToRunQueue(Thread& t)
        {
            Prove<NotOnAnyQueue>(t);

            for (auto& s : sched_runqueue) {
                KASSERT(&s != &t, "thread %p already in runqueue", &t);
                if (s.t_priority <= t.t_priority)
                    continue; // lower or equal priority to thread to insert, skip

                sched_runqueue.insert(s, t);
                return;
            }
            sched_runqueue.push_back(t);
        }

        // Must be called with schedLock held
        void WakeupSleepingThreads()
        {
            if (sched_sleepqueue.empty())
                return;

            Thread& t = sched_sleepqueue.front();
            if ((t.t_flags & THREAD_FLAG_TIMEOUT) == 0)
                return;
            if (time::IsTickBefore(time::GetTicks(), t.t_timeout))
                return;

            sched_sleepqueue.remove(t);
            AddThreadToRunQueue(t);
            t.t_sched_flags &= ~THREAD_SCHED_SUSPENDED;
            t.t_flags &= ~THREAD_FLAG_TIMEOUT;
        }

        // Must be called with schedLock held
        Thread& PickNextThreadToSchedule(Thread& curThread, const int cpuid)
        {
            KASSERT(!sched_runqueue.empty(), "runqueue cannot be empty");
            for (auto& t : sched_runqueue) {
                if (t.t_affinity != thread::AnyAffinity && t.t_affinity != cpuid)
                    continue; // not possible on this CPU
                if (IsActive(t) && &t != &curThread)
                    continue;
                return t;
            }
            panic("nothing on the runqueue for cpu %u", cpuid);
        }

        // Must be called with schedLock held
        void AddThreadToSleepQueue(Thread& t)
        {
            if (t.t_flags & THREAD_FLAG_TIMEOUT) {
                for (auto& st : sched_sleepqueue) {
                    if ((st.t_flags & THREAD_FLAG_TIMEOUT) &&
                        time::IsTickBefore(st.t_timeout, t.t_timeout))
                        continue; // st wakes up earlier than we do
                    sched_sleepqueue.insert(st, t);
                    return;
                }
            }
            sched_sleepqueue.push_back(t);
        }

    } // unnamed namespace

    void InitThread(Thread& t)
    {
        {
            SchedLockGuard g(schedLock);
            Prove<NotOnAnyQueue>(t);

            // New threads are initially suspended
            KASSERT(t.t_state == thread::State::Suspended, "initial thread status not suspended?");
            t.t_sched_flags = THREAD_SCHED_SUSPENDED;
            sched_sleepqueue.push_back(t);
        }
    }

    void ResumeThread(Thread& t)
    {
        SchedLockGuard g(schedLock);

        if (!IsSuspended(t)) {
            // The scheduler never got a chance to actually suspend the
            // thread. This means it is still on the runqueue and we
            // don't need to move it
            Prove<OnlyOnRunQueue>(t);
            return;
        }
        Prove<OnlyOnSleepQueue>(t);

        KASSERT(t.t_sqwaiter.w_sq == nullptr || t.t_sqwaiter.w_signalled, "resuming sleeping thread?");

        sched_sleepqueue.remove(t);
        AddThreadToRunQueue(t);
        t.t_sched_flags &= ~THREAD_SCHED_SUSPENDED;
        t.t_flags &= ~THREAD_FLAG_TIMEOUT;
    }

    extern "C" void scheduler_release(Thread* old)
    {
        SchedLockGuard g(schedLock);

        /* Release the old thread; it is now safe to schedule it elsewhere */
        old->t_sched_flags &= ~THREAD_SCHED_ACTIVE;
    }

    void WaitUntilReleased(Thread& t)
    {
        while((t.t_sched_flags & THREAD_SCHED_ACTIVE) != 0) {
            md::interrupts::Pause();
        }
    }

    void Schedule()
    {
        auto& oldThread = thread::GetCurrent();
        const int cpuid = PCPU_GET(cpuid);

        /*
         * Grab the scheduler lock and disable interrupts; note that they need not be
         * enabled - this happens in interrupt context, which needs to clean up
         * before another interrupt can be handled.
         */
        auto state = schedLock.LockUnpremptible();

        // Update old thread
        oldThread.t_flags &= ~THREAD_FLAG_RESCHEDULE;
        switch(oldThread.t_state) {
            case thread::State::Suspended:
                // Suspended; remove it from the runqueue
                KASSERT(&oldThread != PCPU_GET(idlethread), "suspending idle thread");
                sched_runqueue.remove(oldThread);
                AddThreadToSleepQueue(oldThread);
                oldThread.t_sched_flags |= THREAD_SCHED_SUSPENDED;
                break;
            case thread::State::Running:
                // Achieve round-robin scheduling within the same priority by
                // re-adding the old thread to the run queue
                sched_runqueue.remove(oldThread);
                AddThreadToRunQueue(oldThread);
                break;
            case thread::State::Zombie:
                Prove<OnlyOnRunQueue>(oldThread);
                sched_runqueue.remove(oldThread);
                break;
        }

        /*
         * See if the first item on the sleepqueue is worth waking up; we'll only
         * look at the first item as we expect them to be added in a sorted way.
         */
        WakeupSleepingThreads();

        // Pick the next thread to schedule
        auto& newThread = PickNextThreadToSchedule(oldThread, cpuid);

        // Sanity checks
        KASSERT(!IsSuspended(newThread), "activating suspended thread %p", &newThread);
        KASSERT(
            &newThread == &oldThread || !IsActive(newThread), "activating active thread %p",
            &newThread);
        Prove<OnlyOnRunQueue>(newThread);

        /*
         * Schedule our new thread; by marking it as active, it will not be picked up by another
         * CPU.
         */
        newThread.t_sched_flags |= THREAD_SCHED_ACTIVE;
        PCPU_SET(curthread, &newThread);

        // Now unlock the scheduler lock but do _not_ enable interrupts
        schedLock.Unlock();

        if (&oldThread != &newThread) {
            auto& prev = md::thread::SwitchTo(newThread, oldThread);
            scheduler_release(&prev);
        }

        // Re-enable interrupts if they were
        md::interrupts::Restore(state);
    }

    void Launch()
    {
        Thread* idlethread = PCPU_GET(idlethread);
        KASSERT(&thread::GetCurrent() == idlethread, "idle thread not correct");

        /*
         * Activate the idle thread; the MD startup code should have done the
         * appropriate code/stack switching bits. All we need to do is set up the
         * scheduler enough so that it accepts our idle thread.
         */
        md::interrupts::Disable();
        PCPU_SET(curthread, idlethread);

        /* Run it */
        isActive = true;
        md::interrupts::Enable();
    }

    void Deactivate() { isActive = false; }

    bool IsActive() { return isActive; }

} // namespace scheduler
