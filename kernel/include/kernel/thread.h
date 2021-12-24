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

using kthread_func_t = void(*)(void*);
struct STACKFRAME;
struct Process;
class Result;

namespace thread
{
    constexpr inline auto MaxNameLength = 32;

    enum class State {
        Running,
        Suspended,
        Zombie
    };

    // Priority value, 0 is highest
    using Priority = int;
    constexpr inline Priority DefaultPriority = 200;
    constexpr inline Priority IdlePriority = 255;

    // Affinity is a bitmask of CPU's the thread can be scheduled on
    using Affinity = int;
    constexpr inline Affinity AnyAffinity = -1;

    struct Waiter;
}

struct Thread {
    Thread(Process& process);
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    void SetName(const char* name);

    void Terminate(int); // must be called on curthread
    void Destroy();      // must not be called on curthread

    void Suspend();
    void Resume();

    MD_THREAD_FIELDS // Machine-dependant data

    char t_name[thread::MaxNameLength + 1] = {}; /* Thread name */

    // Scheduler flags are protected by sched_lock
    unsigned int t_sched_flags{};
#define THREAD_SCHED_ACTIVE 0x0001    /* Thread is active on some CPU (curthread==this) */
#define THREAD_SCHED_SUSPENDED 0x0002 /* Thread is currently suspended */

    unsigned int t_flags{};
#define THREAD_FLAG_KTHREAD 0x0001    /* Kernel thread */
#define THREAD_FLAG_RESCHEDULE 0x0002 /* Thread desires a reschedule */
#define THREAD_FLAG_PTRACED 0x0004    /* Thread being ptrace()'d by parent */
#define THREAD_FLAG_TIMEOUT 0x0008    /* Timeout field is valid */

    unsigned int t_sig_pending{};   // Is a signal pending?
    thread::State t_state{thread::State::Suspended};

    struct STACKFRAME* t_frame{};
    unsigned int t_md_flags{};

    Process& t_process; /* associated process */

    thread::Priority t_priority{thread::DefaultPriority};
    thread::Affinity t_affinity{thread::AnyAffinity};

    util::List<Thread>::Node t_NodeAllThreads;
    util::List<Thread>::Node t_NodeSchedulerList;

    /* Timeout, when it expires the thread will be scheduled in */
    tick_t t_timeout{};

    signal::ThreadSpecificData t_sigdata;

    int t_ptrace_sig{};               // Pending ptrace signal, if any

    bool IsRescheduling() const { return (t_flags & THREAD_FLAG_RESCHEDULE) != 0; }

    // Used for threads on the sleepqueue
    util::List<Thread>::Node t_sqchain;
    sleep_queue::Waiter t_sqwaiter;

private:
    ~Thread() = default;
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
