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
#include "kernel/kdb.h"
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
                "prove failed: expected onRunQueue %d, onSleepQueue %d but got onRunQueue %d "
                "onSleepQueue %d",
                What::onRunQueue, What::onSleepQueue, onRunQueue, onSleepQueue);
        }

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
                if (t.IsActive() && &t != &curThread)
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
            t.t_sched_flags = THREAD_SCHED_SUSPENDED;
            sched_sleepqueue.push_back(t);
        }
    }

    void ResumeThread(Thread& t)
    {
        SchedLockGuard g(schedLock);

        // XXX This condition is likely obsoleted...
        if (!t.IsSuspended() && scheduler::IsActive())
            panic("resuming nonsuspended thread %p", &t);

        KASSERT(t.IsSuspended(), "adding non-suspended thread %p", &t);
        Prove<OnlyOnSleepQueue>(t);

        sched_sleepqueue.remove(t);
        AddThreadToRunQueue(t);
        t.t_sched_flags &= ~THREAD_SCHED_SUSPENDED;
        t.t_flags &= ~THREAD_FLAG_TIMEOUT;
    }

    void SuspendThread(Thread& t)
    {
        SchedLockGuard g(schedLock);

        KASSERT(!t.IsSuspended(), "suspending thread %p that is already suspended", &t);
        Prove<OnlyOnRunQueue>(t);

        sched_runqueue.remove(t);
        AddThreadToSleepQueue(t);
        t.t_sched_flags |= THREAD_SCHED_SUSPENDED;
    }

    void ExitThread(Thread& t)
    {
        /*
         * Note that interrupts must be disabled - this is important because we are about to
         * remove the thread from the schedulers runqueue, and it will not be re-added again.
         * Thus, if a context switch would occur, the final exiting code will not be run.
         */
        schedLock.LockUnpremptible();

        Prove<OnlyOnRunQueue>(t);
        sched_runqueue.remove(t);

        /*
         * Turn the thread into a zombie; we'll soon be letting go of the scheduler lock, but all
         * resources are gone and the thread can be destroyed from now on - interrupts are disabled,
         * so we'll be certain to clear the active flag. A thread which is an inactive zombie won't
         * be scheduled anymore because it's on neither runqueue nor sleepqueue; the scheduler won't
         * know about the thread at all.
         */
        t.t_state = thread::State::Zombie;
        /* Let go of the scheduler lock but leave interrupts disabled */
        schedLock.Unlock();

        // Note: we expect the caller to clean up and reschedule() !
    }

    extern "C" void scheduler_release(Thread* old)
    {
        SchedLockGuard g(schedLock);

        /* Release the old thread; it is now safe to schedule it elsewhere */
        old->t_sched_flags &= ~THREAD_SCHED_ACTIVE;
    }

    void Schedule()
    {
        auto& curThread = thread::GetCurrent();
        const int cpuid = PCPU_GET(cpuid);

        /*
         * Grab the scheduler lock and disable interrupts; note that they need not be
         * enabled - this happens in interrupt context, which needs to clean up
         * before another interrupt can be handled.
         */
        auto state = schedLock.LockUnpremptible();

        // Cancel any rescheduling as we are about to schedule here
        curThread.t_flags &= ~THREAD_FLAG_RESCHEDULE;

        /*
         * See if the first item on the sleepqueue is worth waking up; we'll only
         * look at the first item as we expect them to be added in a sorted way.
         */
        WakeupSleepingThreads();

        // Pick the next thread to schedule
        auto& newThread = PickNextThreadToSchedule(curThread, cpuid);

        // Sanity checks
        KASSERT(!newThread.IsSuspended(), "activating suspended thread %p", &newThread);
        KASSERT(
            &newThread == &curThread || !newThread.IsActive(), "activating active thread %p",
            &newThread);
        Prove<OnlyOnRunQueue>(newThread);

        /*
         * If the current thread is not suspended, this means it got interrupted
         * involuntary and must be placed back on the running queue; otherwise it
         * must have been placed on the runqueue already. We'll add it to the back,
         * in order to obtain round-robin scheduling within each priority level.
         *
         * We must also take care not to re-add zombie threads; these must not be
         * re-added to either scheduler queue.
         */
        if (curThread.t_state != thread::State::Suspended && curThread.t_state != thread::State::Zombie) {
            sched_runqueue.remove(curThread);
            AddThreadToRunQueue(curThread);
        }

        /*
         * Schedule our new thread; by marking it as active, it will not be picked up by another
         * CPU.
         */
        newThread.t_sched_flags |= THREAD_SCHED_ACTIVE;
        PCPU_SET(curthread, &newThread);

        // Now unlock the scheduler lock but do _not_ enable interrupts
        schedLock.Unlock();

        if (&curThread != &newThread) {
            auto& prev = md::thread::SwitchTo(newThread, curThread);
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

namespace
{
    void kdbPrintThread(Thread& t)
    {
        kprintf(
            "  thread %p '%s' state %d sched_flags %d flags 0x%x\n", &t, t.t_name, t.t_state, t.t_sched_flags,
            t.t_flags);
        kprintf("    process %d\n", t.t_process.p_pid);
        if (auto& w = t.t_sqwaiter; w.w_sq) {
            kprintf(
                "    sleepqueue %p '%s' thread %p signalled %d\n", w.w_sq, w.w_sq->GetName(),
                w.w_thread, w.w_signalled);
        }
    }
} // namespace

const kdb::RegisterCommand
    kdbScheduler("scheduler", "Display scheduler status", [](int, const kdb::Argument*) {
        kprintf("runqueue\n");
        for (auto& s : scheduler::sched_runqueue) {
            kdbPrintThread(s);
        }
        kprintf("sleepqueue\n");
        for (auto& s : scheduler::sched_sleepqueue) {
            kdbPrintThread(s);
        }
    });
