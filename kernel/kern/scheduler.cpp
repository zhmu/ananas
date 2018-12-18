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
#include "kernel/result.h"
#include "kernel/schedule.h"
#include "kernel/thread.h"
#include "kernel/time.h"
#include "kernel-md/interrupts.h"
#include "kernel-md/md.h"
#include "kernel-md/vm.h"
#include "options.h"

/*
 * The next define adds specific debugger assertions that may incur a
 * significant performance penalty - these assertions are named
 * SCHED_ASSERT.
 */
#define DEBUG_SCHEDULER
#define SCHED_KPRINTF(...)

namespace scheduler
{
    namespace
    {
        bool isActive = false;

        using SchedLock = Spinlock;
        using SchedLockGuard = SpinlockUnpremptibleGuard;

        static SchedLock schedLock;
        static SchedulerPrivList sched_runqueue;
        static SchedulerPrivList sched_sleepqueue;

#ifdef DEBUG_SCHEDULER
        bool IsThreadOnQueue(SchedulerPrivList& q, Thread& t)
        {
            int n = 0;
            for (auto& s : q) {
                if (s.sp_thread == &t)
                    n++;
            }
            return n > 0;
        }
#define SCHED_ASSERT(x, ...) KASSERT((x), __VA_ARGS__)
#else
#define SCHED_ASSERT(x, ...)
#endif

        void AddThreadLocked(Thread& t)
        {
            KASSERT(IsThreadOnQueue(sched_runqueue, t) == 0, "adding thread on runq?");
            KASSERT(IsThreadOnQueue(sched_sleepqueue, t) == 0, "adding thread on sleepq?");

            /*
             * Add it to the runqueue - note that we must preserve order here
             * because the threads must be sorted in order of priority
             * XXX Note that this is O(n) - we can do better
             */
            bool inserted = false;
            for (auto& s : sched_runqueue) {
                KASSERT(s.sp_thread != &t, "thread %p already in runqueue", &t);
                if (s.sp_thread->t_priority <= t.t_priority)
                    continue;

                /* Found a thread with a lower priority; we can insert it here */
                sched_runqueue.insert(s, t.t_sched_priv);
                inserted = true;
                break;
            }
            if (!inserted)
                sched_runqueue.push_back(t.t_sched_priv);
        }

        void WakeupSleepingThreads()
        {
            // Note: must be called with schedLock held
            if (sched_sleepqueue.empty())
                return;

            Thread* t = sched_sleepqueue.front().sp_thread;
            if ((t->t_flags & THREAD_FLAG_TIMEOUT) &&
                time::IsTickAfter(time::GetTicks(), t->t_timeout)) {
                /* Remove the thread from the sleepqueue ... */
                sched_sleepqueue.remove(t->t_sched_priv);
                /* ... and add it to the runqueue ... */
                AddThreadLocked(*t);
                /* ... finally, remove the flags - it's no longer suspended now */
                t->t_flags &= ~(THREAD_FLAG_TIMEOUT | THREAD_FLAG_SUSPENDED);
            }
        }

        Thread& PickNextThreadToSchedule(Thread* curthread, int cpuid)
        {
            // Note: must be called with schedLock held
            KASSERT(!sched_runqueue.empty(), "runqueue cannot be empty");
            SchedulerPriv* next_sched = NULL;
            for (auto& sp : sched_runqueue) {
                /* Skip the thread if we can't schedule it here */
                if (sp.sp_thread->t_affinity != THREAD_AFFINITY_ANY &&
                    sp.sp_thread->t_affinity != cpuid)
                    continue;
                if (sp.sp_thread->IsActive() && sp.sp_thread != curthread)
                    continue;
                next_sched = &sp;
                break;
            }

            KASSERT(next_sched != nullptr, "nothing on the runqueue for cpu %u", cpuid);
            return *next_sched->sp_thread;
        }

    } // unnamed namespace

    void InitThread(Thread& t)
    {
        /* Hook up our private scheduling entity */
        t.t_sched_priv.sp_thread = &t;

        /* Mark the thread as suspened - the scheduler is responsible for this */
        t.t_flags |= THREAD_FLAG_SUSPENDED;

        /* Hook the thread to our sleepqueue */
        {
            SchedLockGuard g(schedLock);
            KASSERT(IsThreadOnQueue(sched_runqueue, t) == 0, "new thread is already on runq?");
            KASSERT(IsThreadOnQueue(sched_sleepqueue, t) == 0, "new thread is already on sleepq?");
            sched_sleepqueue.push_back(t.t_sched_priv);
        }
    }

    void AddThread(Thread& t)
    {
        SCHED_KPRINTF("%s: t=%p\n", __func__, &t);
        SchedLockGuard g(schedLock);

        KASSERT(t.IsSuspended(), "adding non-suspended thread %p", &t);
        SCHED_ASSERT(
            IsThreadOnQueue(sched_runqueue, t) == 0, "adding thread %p already on runqueue", &t);
        SCHED_ASSERT(
            IsThreadOnQueue(sched_sleepqueue, t) == 1, "adding thread %p not on sleepqueue", &t);
        /* Remove the thread from the sleepqueue ... */
        sched_sleepqueue.remove(t.t_sched_priv);
        /* ... and add it to the runqueue ... */
        AddThreadLocked(t);
        /*
         * ... and finally, update the flags: we must do this in the scheduler lock because
         *     no one else is allowed to touch the thread while we're moving it. Note that we
         *     also remove the timeout flag here as the thread is already unsuspended.
         */
        t.t_flags &= ~(THREAD_FLAG_SUSPENDED | THREAD_FLAG_TIMEOUT);
    }

    void RemoveThread(Thread& t)
    {
        SCHED_KPRINTF("%s: t=%p\n", __func__, &t);
        SchedLockGuard g(schedLock);

        KASSERT(!t.IsSuspended(), "removing suspended thread %p", &t);
        SCHED_ASSERT(
            IsThreadOnQueue(sched_sleepqueue, t) == 0, "removing thread already on sleepqueue");
        SCHED_ASSERT(IsThreadOnQueue(sched_runqueue, t) == 1, "removing thread not on runqueue");
        /* Remove the thread from the runqueue ... */
        sched_runqueue.remove(t.t_sched_priv);
        /* ... add it to the sleepqueue ... */
        if (t.t_flags & THREAD_FLAG_TIMEOUT) {
            /* ... but the sleepqueue must be in first-to-wakeup order... */
            bool inserted = false;
            for (auto& s : sched_sleepqueue) {
                Thread* st = s.sp_thread;
                if ((st->t_flags & THREAD_FLAG_TIMEOUT) &&
                    time::IsTickBefore(st->t_timeout, t.t_timeout))
                    continue; /* st wakes up earlier than we do */
                sched_sleepqueue.insert(s, t.t_sched_priv);
                inserted = true;
                break;
            }
            if (!inserted) {
                sched_sleepqueue.push_back(t.t_sched_priv);
            }
        } else {
            sched_sleepqueue.push_back(t.t_sched_priv);
        }
        /*
         * ... and finally, update the flags: we must do this in the scheduler lock because
         *     no one else is allowed to touch the thread while we're moving it
         */
        t.t_flags |= THREAD_FLAG_SUSPENDED;
    }

    void ExitThread(Thread& t)
    {
        /*
         * Note that interrupts must be disabled - this is important because we are about to
         * remove the thread from the schedulers runqueue, and it will not be re-added again.
         * Thus, if a context switch would occur, the final exiting code will not be run.
         */
        schedLock.LockUnpremptible();

        SCHED_ASSERT(
            IsThreadOnQueue(sched_runqueue, t) == 1, "exiting thread already not on sleepqueue");
        SCHED_ASSERT(IsThreadOnQueue(sched_sleepqueue, t) == 0, "exiting thread on runqueue");
        /* Thread seems sane; remove it from the runqueue */
        sched_runqueue.remove(t.t_sched_priv);
        /*
         * Turn the thread into a zombie; we'll soon be letting go of the scheduler lock, but all
         * resources are gone and the thread can be destroyed from now on - interrupts are disabled,
         * so we'll be certain to clear the active flag. A thread which is an inactive zombie won't
         * be scheduled anymore because it's on neither runqueue nor sleepqueue; the scheduler won't
         * know about the thread at all.
         */
        t.t_flags |= THREAD_FLAG_ZOMBIE;
        /* Let go of the scheduler lock but leave interrupts disabled */
        schedLock.Unlock();

        // Note: we expect the caller to clean up and reschedule() !
    }

    extern "C" void scheduler_release(Thread* old)
    {
        /* Release the old thread; it is now safe to schedule it elsewhere */
        SCHED_KPRINTF("old[%p] -active\n", old);
        old->t_flags &= ~THREAD_FLAG_ACTIVE;
    }

    void Schedule()
    {
        Thread* curthread = PCPU_GET(curthread);
        int cpuid = PCPU_GET(cpuid);
        KASSERT(curthread != NULL, "no current thread active");
        SCHED_KPRINTF("schedule(): cpu=%u curthread=%p\n", cpuid, curthread);

        /*
         * Grab the scheduler lock and disable interrupts; note that they need not be
         * enabled - this happens in interrupt context, which needs to clean up
         * before another interrupt can be handled.
         */
        auto state = schedLock.LockUnpremptible();

        // Cancel any rescheduling as we are about to schedule here
        curthread->t_flags &= ~THREAD_FLAG_RESCHEDULE;

        /*
         * See if the first item on the sleepqueue is worth waking up; we'll only
         * look at the first item as we expect them to be added in a sorted way.
         */
        WakeupSleepingThreads();

        // Pick the next thread to schedule
        auto& newthread = PickNextThreadToSchedule(curthread, cpuid);

        // Sanity checks
        KASSERT(!newthread.IsSuspended(), "activating suspended thread %p", &newthread);
        KASSERT(
            &newthread == curthread || !newthread.IsActive(), "activating active thread %p",
            &newthread);
        SCHED_ASSERT(
            IsThreadOnQueue(sched_runqueue, newthread) == 1,
            "scheduling thread not on runqueue (?)");
        SCHED_ASSERT(
            IsThreadOnQueue(sched_sleepqueue, newthread) == 0, "scheduling thread on sleepqueue");

        SCHED_KPRINTF(
            "%s[%d]: newthread=%p curthread=%p\n", __func__, cpuid, &newthread, curthread);

        /*
         * If the current thread is not suspended, this means it got interrupted
         * involuntary and must be placed back on the running queue; otherwise it
         * must have been placed on the runqueue already. We'll add it to the back,
         * in order to obtain round-robin scheduling within each priority level.
         *
         * We must also take care not to re-add zombie threads; these must not be
         * re-added to either scheduler queue.
         */
        if (!curthread->IsSuspended() && !curthread->IsZombie()) {
            SCHED_KPRINTF("%s[%d]: removing t=%p from runqueue\n", __func__, cpuid, curthread);
            sched_runqueue.remove(curthread->t_sched_priv);
            SCHED_KPRINTF("%s[%d]: re-adding t=%p\n", __func__, cpuid, curthread);
            AddThreadLocked(*curthread);
        }

        /*
         * Schedule our new thread; by marking it as active, it will not be picked up by another
         * CPU.
         */
        newthread.t_flags |= THREAD_FLAG_ACTIVE;
        PCPU_SET(curthread, &newthread);

        // Now unlock the scheduler lock but do _not_ enable interrupts
        schedLock.Unlock();

        if (curthread != &newthread) {
            Thread& prev = md::thread::SwitchTo(newthread, *curthread);
            scheduler_release(&prev);
        }

        // Re-enable interrupts if they were
        md::interrupts::Restore(state);
    }

    void Launch()
    {
        Thread* idlethread = PCPU_GET(idlethread);
        KASSERT(PCPU_GET(curthread) == idlethread, "idle thread not correct");

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

#ifdef OPTION_KDB
const kdb::RegisterCommand
    kdbScheduler("scheduler", "Display scheduler status", [](int, const kdb::Argument*) {
        using namespace scheduler;
        kprintf("runqueue\n");
        if (!sched_runqueue.empty()) {
            for (auto& s : sched_runqueue) {
                kprintf("  thread %p\n", s.sp_thread);
            }
        } else {
            kprintf("(empty)\n");
        }
        kprintf("sleepqueue\n");
        if (!sched_sleepqueue.empty()) {
            for (auto& s : sched_sleepqueue) {
                kprintf("  thread %p\n", s.sp_thread);
            }
        } else {
            kprintf("(empty)\n");
        }
    });
#endif /* OPTION_KDB */

/* vim:set ts=2 sw=2: */
