/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/errno.h>
#include <ananas/util/utility.h>
#include "kernel/fd.h"
#include "kernel/init.h"
#include "kernel/kdb.h"
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

/* XXX These should be locked */

namespace process
{
    Mutex process_mtx("process");
    ProcessList process_all;
    Process* process_kernel;

    namespace
    {
        pid_t process_curpid = 0;

        pid_t AllocateProcessID()
        {
            /* XXX this is a bit of a kludge for now ... */
            MutexGuard g(process_mtx);
            return process_curpid++;
        }
    } // unnamed namespace

    void Initialize()
    {
        KASSERT(process_kernel == nullptr, "process already initialised");

        process_kernel = new Process(*kernel_vmspace);
        process_kernel->p_parent = process_kernel;
        process_kernel->p_state = PROCESS_STATE_ACTIVE;
        process_kernel->p_pid = 0;

        {
            MutexGuard g(process::process_mtx);
            process::process_all.push_back(*process_kernel);
            process_curpid = 1;
        }
    }

    Process& GetKernelProcess()
    {
        KASSERT(process_kernel != nullptr, "processes not initialised");
        return *process_kernel;
    }

    Process& GetCurrent()
    {
        auto& t = thread::GetCurrent();
        return t.t_process;
    }
} // namespace process

static Result process_alloc_ex(Process* parent, Process*& dest)
{
    VMSpace* vmspace;
    if (const auto result = vmspace_create(vmspace); result.IsFailure())
        return result;

    auto p = new Process(*vmspace);
    p->p_parent = parent; /* XXX should we take a ref here? */
    p->p_state = PROCESS_STATE_ACTIVE;
    p->p_pid = process::AllocateProcessID();

    // Clone the parent's descriptors
    if (parent != nullptr) {
        for (unsigned int n = 0; n < parent->p_fd.size(); n++) {
            if (parent->p_fd[n] == nullptr)
                continue;

            FD* fd_out;
            fdindex_t index_out;
            if (const auto result = fd::Clone(*parent, n, nullptr, *p, fd_out, n, index_out); result.IsFailure()) {
                delete p;
                return result;
            }
            KASSERT(n == index_out, "cloned fd %d to new fd %d", n, index_out);
        }
    }

    /* Run all process initialization callbacks */
    if (const auto result = vfs_init_process(*p); result.IsFailure()) {
        delete p;
        return result;
    }

    /* Grab the parent's lock and insert the child */
    if (parent != nullptr) {
        parent->Lock();
        parent->p_children.push_back(*p);
        parent->Unlock();
    }

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

Result process_alloc(Process* parent, Process*& dest) { return process_alloc_ex(parent, dest); }

Result Process::Clone(Process*& out_p)
{
    Process* newp;
    if (const auto result = process_alloc_ex(this, newp); result.IsFailure())
        return result;

    /* Duplicate the vmspace - this should leave the private mappings alone */
    if (const auto result = p_vmspace.Clone(newp->p_vmspace); result.IsFailure()) {
        newp->RemoveReference(); // destroys the process
        return result;
    }

    out_p = newp;
    return Result::Success();
}

Process::Process(VMSpace& vs)
    : p_vmspace(vs)
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
    if (p_pid == 1) panic("terminating init!");

    if (p_parent) {
        p_parent->Lock();
        p_parent->p_times.tms_cutime += p_times.tms_utime;
        p_parent->p_times.tms_cstime += p_times.tms_stime;
        p_parent->Unlock();
    }

    // Note that the lock must be held, to prevent a race between Exit() and
    // WaitAndLock()
    p_lock.AssertLocked();
    p_state = PROCESS_STATE_ZOMBIE;
    p_exit_status = status;
}

void Process::SignalExit() { p_parent->p_child_wait.Signal(); }

Result Process::WaitAndLock(int flags, util::locked<Process>& p_out)
{
    if (flags != 0)
        return Result::Failure(EINVAL);

    // Wait for the first zombie child of this process
    for (;;) {
        Lock();
        for (auto& child : p_children) {
            child.Lock();
            if (child.p_state != PROCESS_STATE_ZOMBIE) {
                child.Unlock();
                continue;
            }

            // Found one; remove it from the parent's list
            p_children.remove(child);
            Unlock();

            // Destroy the thread owned by the process
            auto t = child.p_mainthread;
            KASSERT(t != nullptr, "zombie child without main thread?");
            KASSERT(t->IsZombie(), "process zombie without zombie threads?");
            KASSERT(t->t_refcount == 0, "zombie child thread still has %d refs", t->t_refcount);
            t->Destroy();

            // Note that this transfers the parents reference to the caller!
            p_out = util::locked<Process>(child);
            return Result::Success();
        }
        Unlock();

        // Nothing good yet; sleep on it
        p_child_wait.Wait();
    }
    // NOTREACHED
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

const kdb::RegisterCommand kdbPs("ps", "Display all processes", [](int, const kdb::Argument*) {
    MutexGuard g(process::process_mtx);
    for (auto& p : process::process_all) {
        kprintf("process %d (%p): state %d\n", p.p_pid, &p, p.p_state);
        if (auto t = p.p_mainthread; t != nullptr) {
            kprintf("  thread %p flags 0x%x\n", t, t->t_flags);
        }
        //p.p_vmspace.Dump();
    }
});
