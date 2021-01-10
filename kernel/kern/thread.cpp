/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
/*
 * Threads have a current state which is contained in t_flags; possible
 * transitions are:
 *
 *  +-->[suspended]->-+
 *  |       |         |
 *  |       v         |
 *  +-<--[active]     |
 *          |         |
 *          v         |
 *       [zombie]<----+
 *          |
 *          v
 *       [(gone)]
 *
 * All transitions are managed by scheduler.cpp.
 */
#include <ananas/types.h>
#include "kernel/device.h"
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/pcpu.h"
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/schedule.h"
#include "kernel/time.h"
#include "kernel/thread.h"
#include "kernel/vm.h"
#include "kernel-md/interrupts.h"
#include "kernel-md/md.h"
#include "options.h"

namespace
{
    Spinlock spl_threadqueue;
    thread::AllThreadsList allThreads;
} // namespace

Result thread_alloc(Process& p, Thread*& dest, const char* name, int flags)
{
    /* First off, allocate the thread itself */
    auto t = new Thread(p);
    p.AddThread(*t);
    t->t_sched_flags = 0;
    t->t_flags = 0;
    t->t_refcount = 1; /* caller */
    t->SetName(name);

    /* Set up CPU affinity and priority */
    t->t_priority = THREAD_PRIORITY_DEFAULT;
    t->t_affinity = THREAD_AFFINITY_ANY;

    /* Ask machine-dependant bits to initialize our thread data */
    md::thread::InitUserlandThread(*t, flags);

    /* If we don't yet have a main thread, this thread will become the main */
    if (p.p_mainthread == NULL)
        p.p_mainthread = t;

    /* Initialize scheduler-specific parts */
    scheduler::InitThread(*t);

    /* Add the thread to the thread queue */
    {
        SpinlockGuard g(spl_threadqueue);
        allThreads.push_back(*t);
    }

    dest = t;
    return Result::Success();
}

Result kthread_alloc(const char* name, kthread_func_t func, void* arg, Thread*& dest)
{
    /*
     * Kernel threads do not have an associated process, and thus no handles,
     * vmspace and the like.
     */
    auto t = new Thread(process::GetKernelProcess());
    t->t_sched_flags = 0;
    t->t_flags = THREAD_FLAG_KTHREAD;
    t->t_refcount = 1;
    t->t_priority = THREAD_PRIORITY_DEFAULT;
    t->t_affinity = THREAD_AFFINITY_ANY;
    t->SetName(name);

    /* Initialize MD-specifics */
    md::thread::InitKernelThread(*t, func, arg);

    /* Initialize scheduler-specific parts */
    scheduler::InitThread(*t);

    /* Add the thread to the thread queue */
    {
        SpinlockGuard g(spl_threadqueue);
        allThreads.push_back(*t);
    }

    dest = t;
    return Result::Success();
}

/*
 * thread_cleanup() informs everyone about the thread's demise.
 */
static void thread_cleanup(Thread& t)
{
    KASSERT(!t.IsZombie(), "cleaning up zombie thread %p", &t);

    /*
     * Signal anyone waiting on the thread; the terminate information should
     * already be set at this point - note that handle_wait() will do additional
     * checks to ensure the thread is truly gone.
     */
    t.SignalWaiters();
}

Thread::Thread(Process& process)
    : t_process(process)
{
}

/*
 * thread_destroy() take a zombie thread and completely frees it (the thread
 * will not be valid after calling this function and thus this can only be
 * called from a different thread)
 */
void Thread::Destroy()
{
    KASSERT(&thread::GetCurrent() != this, "thread_destroy() on current thread");
    KASSERT(IsZombie(), "thread_destroy() on a non-zombie thread");

    // Wait until the thread is released by the scheduler
    while(IsActive())
        md::interrupts::Pause();

    /* Free the machine-dependant bits */
    md::thread::Free(*this);

    // Unregister ourselves with the owning process
    t_process.RemoveThread(*this);
    {
        SpinlockGuard g(spl_threadqueue);
        allThreads.remove(*this);
    }

    delete this;
}

void Thread::Suspend()
{
    KASSERT(!IsSuspended(), "suspending suspended thread %p", this);
    KASSERT(this != PCPU_GET(idlethread), "suspending idle thread");
    scheduler::SuspendThread(*this);
}

void thread_sleep_ms(unsigned int ms)
{
    unsigned int msPerTick = 1000 / time::GetPeriodicyInHz();
    tick_t num_ticks = ms / msPerTick;
    if (num_ticks == 0)
        num_ticks = 1; // delay at least one tick

    auto& curThread = thread::GetCurrent();
    curThread.t_timeout = time::GetTicks() + num_ticks;
    curThread.t_flags |= THREAD_FLAG_TIMEOUT;
    curThread.Suspend();
    scheduler::Schedule();
}

void Thread::Resume()
{
    scheduler::ResumeThread(*this);
}

void Thread::Terminate(int exitcode)
{
    KASSERT(this == &thread::GetCurrent(), "terminate not on current thread");
    KASSERT(!IsZombie(), "exiting zombie thread");

    // Grab the process lock; this ensures WaitAndLock() will be blocked until the
    // thread is completely forgotten by the scheduler
    t_process.Lock();

    // If we are the process' main thread, mark it as exiting
    if (t_process.p_mainthread == this) {
        t_process.Exit(exitcode);
    }

    // ...
    thread_cleanup(*this);
    --t_refcount;

    // Ask the scheduler to exit the thread (this transitions to zombie-state)
    scheduler::ExitThread(*this);

    // Signal parent in case it is waiting for a child to exit
    t_process.SignalExit();
    t_process.Unlock();

    scheduler::Schedule();
    /* NOTREACHED */
}

void Thread::SetName(const char* name)
{
    if (IsKernel()) {
        /* Surround kernel thread names by [ ] to clearly identify them */
        snprintf(t_name, THREAD_MAX_NAME_LEN, "[%s]", name);
    } else {
        strncpy(t_name, name, THREAD_MAX_NAME_LEN);
    }
    t_name[THREAD_MAX_NAME_LEN] = '\0';
}

Result thread_clone(Process& proc, Thread*& out_thread)
{
    auto& curThread = thread::GetCurrent();

    Thread* t;
    if (auto result = thread_alloc(proc, t, curThread.t_name, THREAD_ALLOC_CLONE);
        result.IsFailure())
        return result;

    /*
     * Must copy the thread state over; note that this is the
     * result of a system call, so we want to influence the
     * return value.
     */
    md::thread::Clone(*t, curThread, 0 /* child gets exit code zero */);

    /* Thread is ready to rock */
    out_thread = t;
    return Result::Success();
}

struct ThreadWaiter : util::List<ThreadWaiter>::NodePtr {
    Semaphore tw_sem{"thread-waiter", 0};
};

void Thread::SignalWaiters()
{
    SpinlockGuard g(t_lock);
    while (!t_waitqueue.empty()) {
        auto& tw = t_waitqueue.front();
        t_waitqueue.pop_front();

        tw.tw_sem.Signal();
    }
}

void Thread::Wait()
{
    ThreadWaiter tw;

    {
        SpinlockGuard g(t_lock);
        t_waitqueue.push_back(tw);
    }

    tw.tw_sem.Wait();
    /* 'tw' will be removed by SignalWaiters() */
}

void idle_thread(void*)
{
    while (1) {
        md::interrupts::Relax();
    }
}

namespace thread {
    Thread& GetCurrent() { return *md_GetCurrentThread(); }
}
