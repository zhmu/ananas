/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
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
#include "kernel/trace.h"
#include "kernel/vm.h"
#include "kernel/vmarea.h"
#include "kernel/vmspace.h"
#include "kernel/vfs/core.h" // for vfs_{init,exit}_process
#include "options.h"

TRACE_SETUP;

/* XXX These should be locked */

namespace process
{
    Mutex process_mtx("process");
    ProcessList process_all;

    namespace
    {
        pid_t process_curpid = 1;

        pid_t AllocateProcessID()
        {
            /* XXX this is a bit of a kludge for now ... */
            MutexGuard g(process_mtx);
            return process_curpid++;
        }

    } // unnamed namespace

} // namespace process

static Result process_alloc_ex(Process* parent, Process*& dest)
{
    auto p = new Process;
    p->p_parent = parent; /* XXX should we take a ref here? */
    p->p_state = PROCESS_STATE_ACTIVE;
    p->p_pid = process::AllocateProcessID();

    /* Create the process's vmspace */
    auto result = vmspace_create(p->p_vmspace);
    if (result.IsFailure()) {
        delete p;
        return result;
    }
    VMSpace& vs = *p->p_vmspace;

    // Clone the parent's descriptors
    if (parent != nullptr) {
        for (unsigned int n = 0; n < parent->p_fd.size(); n++) {
            if (parent->p_fd[n] == nullptr)
                continue;

            FD* fd_out;
            fdindex_t index_out;
            result = fd::Clone(*parent, n, nullptr, *p, fd_out, n, index_out);
            if (result.IsFailure())
                goto fail;
            KASSERT(n == index_out, "cloned fd %d to new fd %d", n, index_out);
        }
    }

    /* Run all process initialization callbacks */
    result = vfs_init_process(*p);
    if (result.IsFailure())
        goto fail;

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

fail:
    vmspace_destroy(*p->p_vmspace);
    delete p;
    return result;
}

Result process_alloc(Process* parent, Process*& dest) { return process_alloc_ex(parent, dest); }

Result Process::Clone(Process*& out_p)
{
    Process* newp;
    if (auto result = process_alloc_ex(this, newp); result.IsFailure())
        return result;

    /* Duplicate the vmspace - this should leave the private mappings alone */
    if (auto result = p_vmspace->Clone(*newp->p_vmspace); result.IsFailure()) {
        newp->RemoveReference(); // destroys the process
        return result;
    }

    out_p = newp;
    return Result::Success();
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
    vmspace_destroy(*p_vmspace);

    // Remove the process from the all-process list
    {
        MutexGuard g(process::process_mtx);
        process::process_all.remove(*this);
    }
}

void Process::RegisterThread(Thread& t)
{
    KASSERT(
        t.t_process == nullptr, "thread %p already registered to process %p (we are %p)", &t,
        t.t_process, this);
    AddReference();

    p_lock.AssertLocked();
    t.t_process = this;
    if (p_mainthread == nullptr)
        p_mainthread = &t;
}

void Process::UnregisterThread(Thread& t)
{
    KASSERT(
        t.t_process == this, "thread %p does not belongs to process %p, not %p!", &t, t.t_process,
        this);
    RemoveReference();

    // There must be at least one more reference to the process if the last
    // thread dies: whoever calls wait()
    p_lock.AssertLocked();
    t.t_process = nullptr;
    if (p_mainthread == &t)
        p_mainthread = nullptr;
}

void Process::Exit(int status)
{
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
        return RESULT_MAKE_FAILURE(EINVAL);

    // Every process has a semaphore used to wait for children XXX This will
    // still give problems if multiple threads in the same parent are waiting for
    // children!
    for (;;) {
        Lock();
        for (auto& child : p_children) {
            child.Lock();
            if (child.p_state == PROCESS_STATE_ZOMBIE) {
                // Found one; remove it from the parent's list
                p_children.remove(child);
                Unlock();

                /*
                 * Deref the main thread; this should kill it as there is nothing else
                 * left (all other references are gone by this point, as it is a zombie)
                 */
                KASSERT(child.p_mainthread != nullptr, "zombie child without main thread?");
                KASSERT(
                    child.p_mainthread->t_refcount == 1,
                    "zombie child main thread still has %d refs", child.p_mainthread->t_refcount);
                child.p_mainthread->Deref();

                // Note that this transfers the parents reference to the caller!
                p_out = util::locked<Process>(child);
                return Result::Success();
            }
            child.Unlock();
        }
        Unlock();

        /* Nothing good yet; sleep on it */
        p_child_wait.Wait();
    }

    /* NOTREACHED */
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

#ifdef OPTION_KDB
const kdb::RegisterCommand kdbPs("ps", "Display all processes", [](int, const kdb::Argument*) {
    MutexGuard g(process::process_mtx);
    for (auto& p : process::process_all) {
        kprintf("process %d (%p): state %d\n", p.p_pid, &p, p.p_state);
        p.p_vmspace->Dump();
    }
});
#endif // OPTION_KDB
