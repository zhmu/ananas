/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
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
    t->SetName(name);

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
    // Give every kthread its own process so we can do proper accounting
    auto& proc = process::AllocateKernelProcess();

    auto t = new Thread(proc);
    t->t_flags = THREAD_FLAG_KTHREAD;
    t->SetName(name);

    proc.AddThread(*t);
    proc.Unlock();

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
    KASSERT(t_state == thread::State::Zombie, "thread_destroy() on a non-zombie thread");

    // Wait until the thread is released by the scheduler - this ensures the
    // thread isn't running on another CPU and makes it safe to free the kernel
    // stack
    scheduler::WaitUntilReleased(*this);

    // Free the machine-dependant bits
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
    KASSERT(this != PCPU_GET(idlethread), "suspending idle thread");
    KASSERT(t_state == thread::State::Running, "suspending non-running thread");

    t_state = thread::State::Suspended;
    // No need to inform the scheduler as it will do the proper handling once
    // it scheduled away from the now-suspended thread
}

void thread_sleep_ms(unsigned int ms)
{
    unsigned int msPerTick = 1000 / time::GetPeriodicyInHz();
    tick_t num_ticks = ms / msPerTick;
    if (num_ticks == 0)
        num_ticks = 1; // delay at least one tick
    thread::SleepUntilTick(time::GetTicks() + num_ticks);
}

void Thread::Resume()
{
    KASSERT(t_state == thread::State::Suspended, "resuming non-suspended thread %p", this);
    t_state = thread::State::Running;
    scheduler::ResumeThread(*this);
}

void Thread::Terminate(int exitcode)
{
    KASSERT(this == &thread::GetCurrent(), "terminate not on current thread");
    KASSERT(t_state == thread::State::Running, "terminating non-running thread");

    // Grab the process lock; this ensures WaitAndLock() will be blocked until the
    // thread is completely forgotten by the scheduler
    t_process.Lock();

    // If we are the process' main thread, mark it as exiting
    if (t_process.p_mainthread == this) {
        t_process.Exit(exitcode);
    }

    md::interrupts::Disable();

    // Mark the thread as a zombie - we are running with interrupts disabled, so we can't
    // get pre-empted away
    t_state = thread::State::Zombie;

    // Signal parent in case it is waiting for a child to exit
    t_process.Unlock();

    scheduler::Schedule();
    __builtin_unreachable();
}

void Thread::SetName(const char* name)
{
    if (t_flags & THREAD_FLAG_KTHREAD) {
        /* Surround kernel thread names by [ ] to clearly identify them */
        snprintf(t_name, thread::MaxNameLength, "[%s]", name);
    } else {
        strncpy(t_name, name, thread::MaxNameLength);
    }
    t_name[thread::MaxNameLength] = '\0';
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

void idle_thread(void*)
{
    while (1) {
        md::interrupts::Relax();
    }
}

namespace thread {
    Thread& GetCurrent() { return *md_GetCurrentThread(); }

    void SleepUntilTick(const tick_t deadline)
    {
        auto& curThread = GetCurrent();
        curThread.t_timeout = deadline;
        curThread.t_flags |= THREAD_FLAG_TIMEOUT;
        curThread.Suspend();
        scheduler::Schedule();
    }
}
