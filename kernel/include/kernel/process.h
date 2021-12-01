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
#include "kernel/lock.h"
#include "kernel/shm.h" // for ProcessSpecificData

struct DEntry;
class Result;

struct FD;
struct Thread;
class VMSpace;

#define PROCESS_STATE_ACTIVE 1
#define PROCESS_STATE_ZOMBIE 2

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

    using ChildrenList = util::List<::Process, internal::ProcessAllNodeAccessor<::Process>>;
    using ProcessList = util::List<::Process, internal::ProcessChildrenNodeAccessor<::Process>>;
    using GroupMemberList = util::List<::Process, internal::ProcessGroupNodeAccessor<::Process>>;

    // Callbacks so we can organise things when needed
    typedef Result (*process_callback_t)(Process& proc);
    struct Callback : util::List<Callback>::NodePtr {
        Callback(process_callback_t func) : pc_func(func) {}
        process_callback_t pc_func;
    };
    using CallbackList = util::List<Callback>;

    struct ProcessGroup;
    void Initialize();
    Process& GetKernelProcess();
    Process& GetCurrent();

    enum class TickContext { Userland, Kernel };
} // namespace process

struct Process final : util::refcounted<Process> {
    Process(VMSpace&);
    ~Process();

    void AssertLocked() { p_lock.AssertLocked(); }

    void Lock() { p_lock.Lock(); }

    void Unlock() { p_lock.Unlock(); }

    unsigned int p_state = 0; /* Process state */

    pid_t p_pid = 0;       /* Process ID */
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
    void SignalExit();

    void AddThread(Thread& t);
    void RemoveThread(Thread& t);

    Result Clone(Process*& out_p);

    Result WaitAndLock(int flags, util::locked<Process>& p_out); // transfers reference to caller!

  private:
    Mutex p_lock{"plock"}; /* Locks the process */
    Semaphore p_child_wait{"pchildwait", 0};
};

Result process_alloc(Process* parent, Process*& dest);

Process* process_lookup_by_id_and_lock(pid_t pid);
