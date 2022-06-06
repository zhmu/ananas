/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/errno.h>
#include "kernel/fd.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/lock.h"
#include "kernel/process.h"
#include "kernel/result.h"

namespace fd
{
    namespace
    {
        constexpr size_t numberOfDescriptors = 500; // XXX shouldn't be static

        util::List<FD> fdFreeList;
        Spinlock spl_dqueue;

        util::List<FDType> fdTypes;
        Spinlock spl_fdtypes;

    } // unnamed namespace

    void Initialize()
    {
        auto h = new FD[numberOfDescriptors];
        for (unsigned int i = 0; i < numberOfDescriptors; i++, h++) {
            fdFreeList.push_back(*h);
        }
    }

    Result
    Allocate(int type, Process& proc, fdindex_t index_from, FD*& fd_out, fdindex_t& index_out)
    {
        /* Look up the descriptor type XXX O(n) - should be an array */
        FDType* dtype = nullptr;
        {
            SpinlockGuard g(spl_fdtypes);
            for (auto& dt : fdTypes) {
                if (dt.ft_id != type)
                    continue;
                dtype = &dt;
                break;
            }
        }
        if (dtype == nullptr)
            return Result::Failure(EINVAL);

        // Grab a descriptor from the free list
        FD& fd = []() -> FD& {
            SpinlockGuard g(spl_dqueue);
            if (fdFreeList.empty()) {
                /* XXX should we wait for a new descriptor, or just give an error? */
                panic("out of descriptors");
            }
            auto& fd = fdFreeList.front();
            fdFreeList.pop_front();
            KASSERT(fd.fd_type == FD_TYPE_UNUSED, "descriptor from freelist is not free");
            return fd;
        }();

        // Initialize the descriptor
        fd.fd_type = type;
        fd.fd_process = &proc;
        fd.fd_ops = &dtype->ft_ops;
        fd.fd_flags = 0;

        // Hook the descriptor to the process
        auto n = index_from;
        {
            proc.Lock();
            while (n < PROCESS_MAX_DESCRIPTORS && proc.p_fd[n] != nullptr)
                n++;
            if (n < PROCESS_MAX_DESCRIPTORS)
                proc.p_fd[n] = &fd;
            proc.Unlock();
        }
        if (n == PROCESS_MAX_DESCRIPTORS)
            panic("out of descriptors"); // XXX FIX ME

        fd_out = &fd;
        index_out = n;
        return Result::Success();
    }

    Result Lookup(Process& proc, fdindex_t index, int type, FD*& fd_out)
    {
        if (index < 0 || index >= proc.p_fd.size())
            return Result::Failure(EINVAL);

        // Obtain the descriptor XXX How do we ensure it won't get freed after this? Should we ref
        // it?
        auto fd = [&](int index) {
            proc.Lock();
            auto fd = proc.p_fd[index];
            proc.Unlock();
            return fd;
        }(index);

        // Ensure descriptor exists - we don't verify ownership: it _is_ in the process' descriptor
        // table...
        if (fd == nullptr)
            return Result::Failure(EINVAL);
        // Verify the type...
        if (type != FD_TYPE_ANY && fd->fd_type != type)
            return Result::Failure(EINVAL);
        // ... yet unused descriptors are never valid */
        if (fd->fd_type == FD_TYPE_UNUSED)
            return Result::Failure(EINVAL);
        fd_out = fd;
        return Result::Success();
    }

    Result FreeByIndex(Process& proc, fdindex_t index)
    {
        FD* fd;
        if (auto result = Lookup(proc, index, FD_TYPE_ANY, fd); result.IsFailure())
            return result;

        return fd->Close();
    }

    Result CloneGeneric(
        FD& fd_in, Process& proc_out, FD*& fd_out, fdindex_t index_out_min, fdindex_t& index_out)
    {
        if (auto result = Allocate(fd_in.fd_type, proc_out, index_out_min, fd_out, index_out);
            result.IsFailure())
            return result;

        memcpy(&fd_out->fd_data, &fd_in.fd_data, sizeof(fd_in.fd_data));
        return Result::Success();
    }

    Result Clone(
        Process& proc_in, fdindex_t index_in, struct CLONE_OPTIONS* opts, Process& proc_out,
        FD*& fd_out, fdindex_t index_out_min, fdindex_t& index_out)
    {
        FD* fd_in;
        if (auto result = Lookup(proc_in, index_in, FD_TYPE_ANY, fd_in); result.IsFailure())
            return result;

        MutexGuard g(fd_in->fd_mutex);
        if (fd_in->fd_ops->d_clone != NULL)
            return fd_in->fd_ops->d_clone(
                proc_in, index_in, *fd_in, opts, proc_out, fd_out, index_out_min, index_out);
        return Result::Failure(EINVAL);
    }

    void RegisterType(FDType& ft)
    {
        SpinlockGuard g(spl_fdtypes);
        fdTypes.push_back(ft);
    }

    void UnregisterType(FDType& ft)
    {
        SpinlockGuard g(spl_fdtypes);
        fdTypes.remove(ft);
    }

} // namespace fd

Result FD::Close()
{
    /*
     * Lock the descriptor so that no-one else can touch it, and mark it as
     * torn-down and see if we actually have to destroy it at this point.
     */
    fd_mutex.Lock();

    // Remove us from the process descriptor queue, if necessary
    Process* proc = fd_process;
    if (proc != nullptr) {
        proc->Lock();
        for (auto& d : proc->p_fd) {
            if (d == this)
                d = nullptr;
        }
        proc->Unlock();
    }

    // If the descriptor has a specific free function, call it - otherwise assume
    // no special action is needed.
    if (fd_ops->d_free != nullptr)
        if (auto result = fd_ops->d_free(*proc, *this); result.IsFailure())
            return result;

    // Clear our data
    memset(&fd_data, 0, sizeof(fd_data));
    fd_type = FD_TYPE_UNUSED;
    fd_process = nullptr;

    /*
     * Let go of the descriptor lock - if someone tries to use it, they'll lock
     * it before looking at the flags field and this will cause a deadlock.
     * Better safe than sorry.
     */
    fd_mutex.Unlock();

    // Put the descriptor back to the the pool
    {
        SpinlockGuard g(fd::spl_dqueue);
        fd::fdFreeList.push_back(*this);
    }
    return Result::Success();
}
