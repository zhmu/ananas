/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>
#include <ananas/util/list.h>
#include "kernel/page.h"
#include "kernel/refcount.h"
#include "kernel/schedule.h"
#include "kernel/sleepqueue.h" // for sleep_queue::Waiter
#include "kernel/thread_fwd.h"
#include "kernel/signal.h" // for ThreadSpecificData
#include "kernel/vfs/generic.h"
#include "kernel-md/thread.h"

typedef void (*kthread_func_t)(void*);
struct STACKFRAME;
struct Process;
class Result;

#define THREAD_MAX_NAME_LEN 32
#define THREAD_EVENT_EXIT 1

struct Thread;
struct ThreadWaiter;
typedef util::List<ThreadWaiter> ThreadWaiterList;

struct Thread {
    Thread(Process& process);

    void SetName(const char* name);

    void Terminate(int); // must be called on curthread
    void Destroy();      // must not be called on curthread

    void Suspend();
    void Resume();

    void SignalWaiters();
    void Wait();

    /* Machine-dependant data - must be first */
    MD_THREAD_FIELDS

    Spinlock t_lock;                      /* Lock protecting the thread data */
    char t_name[THREAD_MAX_NAME_LEN + 1] = {}; /* Thread name */

    refcount_t t_refcount{}; /* Reference count of the thread, >0 */

    // Scheduler flags are protected by sched_lock
    unsigned int t_sched_flags{};
#define THREAD_SCHED_ACTIVE 0x0001    /* Thread is active on some CPU (curthread==this) */
#define THREAD_SCHED_SUSPENDED 0x0002 /* Thread is currently suspended */

    unsigned int t_flags{};
#define THREAD_FLAG_ZOMBIE 0x0004     /* Thread has no more resources */
#define THREAD_FLAG_RESCHEDULE 0x0008 /* Thread desires a reschedule */
#define THREAD_FLAG_TIMEOUT 0x0020    /* Timeout field is valid */
#define THREAD_FLAG_SIGPENDING 0x0040 /* Signal is pending */
#define THREAD_FLAG_KTHREAD 0x8000    /* Kernel thread */

    struct STACKFRAME* t_frame{};
    unsigned int t_md_flags{};

    Process& t_process; /* associated process */

    int t_priority{}; /* priority (0 highest) */
#define THREAD_PRIORITY_DEFAULT 200
#define THREAD_PRIORITY_IDLE 255
    int t_affinity{}; /* thread CPU */
#define THREAD_AFFINITY_ANY -1

    util::List<Thread>::Node t_NodeAllThreads;
    util::List<Thread>::Node t_NodeSchedulerList;

    /* Waiters to signal on thread changes */
    ThreadWaiterList t_waitqueue;

    /* Timeout, when it expires the thread will be scheduled in */
    tick_t t_timeout{};

    signal::ThreadSpecificData t_sigdata;

    bool IsActive() const { return (t_sched_flags & THREAD_SCHED_ACTIVE) != 0; }

    bool IsSuspended() const { return (t_sched_flags & THREAD_SCHED_SUSPENDED) != 0; }

    bool IsZombie() const { return (t_flags & THREAD_FLAG_ZOMBIE) != 0; }

    bool IsRescheduling() const { return (t_flags & THREAD_FLAG_RESCHEDULE) != 0; }

    bool IsKernel() const { return (t_flags & THREAD_FLAG_KTHREAD) != 0; }

    // Used for threads on the sleepqueue
    util::List<Thread>::Node t_sqchain;
    sleep_queue::Waiter t_sqwaiter;
};

Result kthread_alloc(const char* name, kthread_func_t func, void* arg, Thread*& dest);

#define THREAD_ALLOC_DEFAULT 0 /* Nothing special */
#define THREAD_ALLOC_CLONE 1   /* Thread is created for cloning */

Result thread_alloc(Process& p, Thread*& dest, const char* name, int flags);

void idle_thread(void*);

void thread_sleep_ms(unsigned int ms);
void thread_dump(int num_args, char** arg);
Result thread_clone(Process& proc, Thread*& dest);

namespace thread {
    Thread& GetCurrent();
    void SleepUntilTick(const tick_t deadline);
}
