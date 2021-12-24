/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/limits.h>
#include <ananas/util/array.h>
#include <ananas/util/list.h>
#include <ananas/util/locked.h>
#include <ananas/util/refcounted.h>
#include <ananas/tms.h>
#include "kernel/condvar.h"
#include "kernel/lock.h"
#include "kernel/types.h"
#include "kernel/shm.h" // for ProcessSpecificData

struct DEntry;
class Result;

struct FD;
struct Thread;
class VMSpace;

struct Process;

namespace process
{
    // Internal stuff so we can work with children and all nodes
    namespace internal
    {
        template<typename T>
        struct AllNode {
            static typename util::List<T>::Node& Get(T& t) { return t.p_NodeAll; }
        };

        template<typename T>
        struct ChildrenNode {
            static typename util::List<T>::Node& Get(T& t) { return t.p_NodeChildren; }
        };

        template<typename T>
        struct GroupNode {
            static typename util::List<T>::Node& Get(T& t) { return t.p_NodeGroup; }
        };

        template<typename T>
        using ProcessAllNodeAccessor =
            typename util::List<T>::template nodeptr_accessor<AllNode<T>>;
        template<typename T>
        using ProcessChildrenNodeAccessor =
            typename util::List<T>::template nodeptr_accessor<ChildrenNode<T>>;
        template<typename T>
        using ProcessGroupNodeAccessor =
            typename util::List<T>::template nodeptr_accessor<GroupNode<T>>;

    } // namespace internal

    using ChildrenList = util::List<::Process, internal::ProcessChildrenNodeAccessor<::Process>>;
    using ProcessList = util::List<::Process, internal::ProcessAllNodeAccessor<::Process>>;
    using GroupMemberList = util::List<::Process, internal::ProcessGroupNodeAccessor<::Process>>;

    // Callbacks so we can organise things when needed
    typedef Result (*process_callback_t)(Process& proc);
    struct Callback : util::List<Callback>::NodePtr {
        Callback(process_callback_t func) : pc_func(func) {}
        process_callback_t pc_func;
    };
    using CallbackList = util::List<Callback>;

    struct ProcessGroup;
    Process& AllocateKernelProcess();
    Process& CreateInitProcess();
    Process& GetCurrent();

    enum class TickContext { Userland, Kernel };
} // namespace process

struct Process final : util::refcounted<Process> {
    Process(pid_t, VMSpace&);
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    void AssertLocked() { p_lock.AssertLocked(); }

    void Lock() { p_lock.Lock(); }

    void Unlock() { p_lock.Unlock(); }

    const pid_t p_pid;     /* Process ID */
    int p_exit_status = 0; /* Exit status / code */

    Process* p_parent = nullptr;  /* Parent process, if any */
    VMSpace& p_vmspace; /* Process memory space */

    Thread* p_mainthread = nullptr; /* Main thread */

    util::array<FD*, PROCESS_MAX_DESCRIPTORS> p_fd = {0}; /* Descriptors */

    DEntry* p_cwd = nullptr; /* Current path */

    process::ChildrenList p_children; // List of this process' children

    process::ProcessGroup* p_group = nullptr;
    shm::ProcessSpecificData p_shm;

    struct tms p_times{};

    // Node pointers to maintain membership of several lists
    util::List<Process>::Node p_NodeAll;
    util::List<Process>::Node p_NodeChildren;
    util::List<Process>::Node p_NodeGroup;

    void OnTick(process::TickContext);
    void Exit(int status);

    void AddThread(Thread& t);
    void RemoveThread(Thread& t);

    Result Clone(Process*& out_p);

    Result WaitAndLock(pid_t, int flags, util::locked<Process>& p_out); // transfers reference to caller!

    void ReparentToInit();

  private:
    Mutex p_lock{"plock"}; /* Locks the process */

    friend class util::detail::default_refcounted_cleanup<Process>;
    ~Process();
};

Process* process_lookup_by_id_and_lock(pid_t pid);
