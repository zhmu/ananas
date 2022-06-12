/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/errno.h>
#include <ananas/wait.h>
#include <ananas/util/utility.h>
#include "kernel/fd.h"
#include "kernel/init.h"
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/process.h"
#include "kernel/processgroup.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/vm.h"
#include "kernel/vmarea.h"
#include "kernel/vmspace.h"
#include "kernel/vfs/core.h" // for vfs_{init,exit}_process

namespace process
{
    Mutex process_mtx("process");
    ProcessList process_all;
}

namespace
{
    constexpr auto inline initPid = 1;
    pid_t process_curpid = initPid + 1;
    Process* initProcess = nullptr;

    pid_t AllocateProcessID()
    {
        /* XXX this is a bit of a kludge for now ... */
        MutexGuard g(process::process_mtx);
        return process_curpid++;
    }

    Result AllocateProcess(Process* parent, Process*& dest, pid_t pid)
    {
        VMSpace* vmspace;
        if (const auto result = vmspace_create(vmspace); result.IsFailure())
            return result;

        auto p = new Process(pid, *vmspace);

        if (parent != nullptr) {
            // Clone the parent's descriptors
            for (unsigned int n = 0; n < parent->p_fd.size(); n++) {
                if (parent->p_fd[n] == nullptr)
                    continue;

                FD* fd_out;
                fdindex_t index_out;
                if (const auto result = fd::Clone(*parent, n, nullptr, *p, fd_out, n, index_out); result.IsFailure()) {
                    p->RemoveReference();
                    return result;
                }
                KASSERT(n == index_out, "cloned fd %d to new fd %d", n, index_out);
            }

            // Grab the parent's lock and insert the child
            parent->AddReference();
            parent->Lock();
            parent->p_children.push_back(*p);
            parent->Unlock();
            p->p_parent = parent;
        }

        /* Run all process initialization callbacks */
        if (const auto result = vfs_init_process(*p); result.IsFailure())
            panic("vfs_init_process is not supposed to fail %d", result.AsStatusCode());

        process::InitializeProcessGroup(*p, parent);

        // Grab the process right before adding it to the list to ensure
        // no one can modify it while it is being set up
        p->Lock();

        /* Finally, add the process to all processes */
        {
            MutexGuard g(process::process_mtx);
            process::process_all.push_back(*p);
        }

        dest = p;
        return Result::Success();
    }
} // unnamed namespace

namespace process
{
    Process& GetCurrent()
    {
        auto& t = thread::GetCurrent();
        return t.t_process;
    }

    Process& AllocateKernelProcess()
    {
        auto p = new Process(AllocateProcessID(), *kernel_vmspace);
        if (initProcess) {
            p->p_parent = initProcess;
            initProcess->Lock();
            initProcess->p_children.push_back(*p);
            initProcess->Unlock();
        }

        p->Lock();
        {
            MutexGuard g(process::process_mtx);
            process::process_all.push_back(*p);
        }
        return *p;
    }

    Process& CreateInitProcess()
    {
        Process* proc;
        if (auto result = AllocateProcess(nullptr, proc, initPid); result.IsFailure())
            panic("cannot create init process %d", result.AsStatusCode());

        Thread* t;
        if (auto result = thread_alloc(*proc, t, "init", THREAD_ALLOC_DEFAULT); result.IsFailure())
            panic("cannot create init thread %d", result.AsStatusCode());
        proc->Unlock();
        initProcess = proc;

        // Now that we have an init process, parent all kernel processes to it
        {
            MutexGuard g(process::process_mtx);
            for (auto& p: process::process_all) {
                p.ReparentToInit();
            }
        }

        return *proc;
    }
}

Result Process::Clone(Process*& out_p)
{
    Process* newp;
    if (const auto result = AllocateProcess(this, newp, AllocateProcessID()); result.IsFailure())
        return result;

    /* Duplicate the vmspace - this should leave the private mappings alone */
    if (const auto result = p_vmspace.Clone(newp->p_vmspace); result.IsFailure()) {
        newp->RemoveReference(); // destroys the process
        return result;
    }

    out_p = newp;
    return Result::Success();
}

Process::Process(pid_t pid, VMSpace& vs)
    : p_pid(pid), p_vmspace(vs)
{
}

Process::~Process()
{
    // XXX This is crude: we need to drop the lock because Fd::Close() may lock/unlock the
    // process. This needs extra thought.
    p_lock.AssertLocked();
    Unlock();

    // Run all process exit callbacks
    vfs_exit_process(*this);

    // Free all descriptors
    for (auto& d : p_fd) {
        if (d == nullptr)
            continue;
        d->Close();
        d = nullptr;
    }

    process::AbandonProcessGroup(*this);

    // Process is gone - destroy any memory mappings held by it
    if (&p_vmspace != kernel_vmspace)
        vmspace_destroy(p_vmspace);

    // Remove the process from the all-process list
    {
        MutexGuard g(process::process_mtx);
        process::process_all.remove(*this);
    }
}

void Process::AddThread(Thread& t)
{
    KASSERT(p_mainthread == nullptr, "process %d already has thread %p", p_pid, p_mainthread);
    AddReference();

    p_lock.AssertLocked();
    p_mainthread = &t;
}

void Process::RemoveThread(Thread& t)
{
    KASSERT(p_mainthread == &t, "process %d thread is not %p but %p", p_pid, &t, p_mainthread);
    RemoveReference();

    // There must be at least one more reference to the process if the last
    // thread dies: whoever calls wait()
    p_lock.AssertLocked();
    p_mainthread = nullptr;
}

void Process::Exit(int status)
{
    if (p_pid == initPid) panic("terminating init!");

    if (p_parent) {
        //if (p_parent != this) p_parent->Lock();
        p_parent->p_times.tms_cutime += p_times.tms_utime;
        p_parent->p_times.tms_cstime += p_times.tms_stime;
        //if (p_parent != this) p_parent->Unlock();
    }

    for (auto& child : p_children) {
        child.ReparentToInit();
    }

    // Note that the lock must be held, to prevent a race between Exit() and
    // WaitAndLock()
    p_lock.AssertLocked();
    p_exit_status = status;
}

Result Process::WaitAndLock(pid_t pid, int flags, util::locked<Process>& p_out)
{
    p_lock.Lock();
    for (;;) {
        for (auto& child : p_children) {
            if (&child == initProcess) continue;
            KASSERT(&child != this, "child of self?");
            if (pid > 0 && pid != child.p_pid)
                continue; // not the child the caller asked for
            child.Lock();
            auto t = child.p_mainthread;
            KASSERT(t != nullptr, "child without a main thread?");

            if (t->t_state != thread::State::Zombie) {
                child.Unlock();
                continue;
            }

            // Found one; remove it from the parent's list
            p_children.remove(child);
            RemoveReference(); // the child no longer refers to us
            p_lock.Unlock();

            // Destroy the thread owned by the process
            t->Destroy();

            // Note that this transfers the parents reference to the caller!
            p_out = util::locked<Process>(child);
            return Result::Success();
        }

        if (flags & WNOHANG) {
            p_lock.Unlock();
            return Result::Failure(ECHILD);
        }

        // Nothing yet - schedule away and see if we have better luck next
        // time. This could maybe be implemented using a condition variable or
        // similar, but this is very tricky since setting the thread state to
        // Zombie means the scheduler will _never_ pick the thread up again,
        // so we can't reliably do anything in between (and we need to have
        // the Zombie condition here as only then can we destroy a thread,
        // provided the scheduler isn't active on it...)
        p_lock.Unlock();
        thread_sleep_ms(1);
        p_lock.Lock();
    }
}

void Process::OnTick(process::TickContext tc)
{
    switch(tc) {
        case process::TickContext::Userland:
            ++p_times.tms_utime;
            break;
        case process::TickContext::Kernel:
            ++p_times.tms_stime;
            break;
    }
}

void Process::ReparentToInit()
{
    KASSERT(initProcess != nullptr, "no init process");

    Lock();
    if (p_parent == initProcess) {
        Unlock();
        return;
    }

    if (p_parent) {
        p_parent->Lock();
        p_parent->p_children.remove(*this);
        p_parent->Unlock();
        p_parent->RemoveReference();
    }

#if 1
    p_parent = initProcess;
    p_parent->AddReference();
    if (p_parent != this)
        p_parent->Lock();
    p_parent->p_children.push_back(*this);
    if (p_parent != this)
        p_parent->Unlock();
#endif
    Unlock();
}

Process* process_lookup_by_id_and_lock(pid_t pid)
{
    MutexGuard g(process::process_mtx);

    for (auto& p : process::process_all) {
        p.Lock();
        if (p.p_pid != pid) {
            p.Unlock();
            continue;
        }
        return &p;
    }
    return nullptr;
}
