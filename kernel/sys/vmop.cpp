/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/syscalls.h>
#include <ananas/syscall-vmops.h>
#include "kernel/fd.h"
#include "kernel/device.h"
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/vm.h"
#include "kernel/vmarea.h"
#include "kernel/vmspace.h"
#include "kernel/vfs/dentry.h"
#include "syscall.h"

namespace
{
    Result sys_vmop_map_fd(
        Thread* curthread, struct VMOP_OPTIONS* vo, VMSpace& vs, int vm_flags, addr_t dest_addr,
        VMArea*& va)
    {
        FD* fd;
        if (auto result = syscall_get_fd(*curthread, FD_TYPE_FILE, vo->vo_fd, fd);
            result.IsFailure()) {
            return result;
        }
        if (auto device = fd->fd_data.d_vfs_file.f_device; device) {
            if (device == nullptr)
                return Result::Failure(EINVAL);
            size_t length = vo->vo_len;
            vm_flags &= ~(VM_FLAG_FAULT | VM_FLAG_EXECUTE);
            addr_t physAddress;
            if (auto result = device->GetDeviceOperations().DetermineDevicePhysicalAddres(
                    physAddress, length, vm_flags);
                result.IsFailure())
                return result;
            return vs.Map(dest_addr, physAddress, length, vm_flags, vm_flags, va);
        }

        if (auto dentry = fd->fd_data.d_vfs_file.f_dentry; dentry) {
            return vs.MapToDentry(
                dest_addr, vo->vo_len, *dentry, vo->vo_offset, vo->vo_len, vm_flags, va);
        }

        return Result::Failure(EINVAL);
    }

    Result sys_vmop_map(Thread* curthread, struct VMOP_OPTIONS* vo)
    {
        if (vo->vo_len == 0)
            return Result::Failure(EINVAL);
        if ((vo->vo_flags & (VMOP_FLAG_PRIVATE | VMOP_FLAG_SHARED)) == 0)
            return Result::Failure(EINVAL);

        int vm_flags = VM_FLAG_USER | VM_FLAG_FAULT;
        if (vo->vo_flags & VMOP_FLAG_READ)
            vm_flags |= VM_FLAG_READ;
        if (vo->vo_flags & VMOP_FLAG_WRITE)
            vm_flags |= VM_FLAG_WRITE;
        if (vo->vo_flags & VMOP_FLAG_EXECUTE)
            vm_flags |= VM_FLAG_EXECUTE;
        if (vo->vo_flags & VMOP_FLAG_PRIVATE)
            vm_flags |= VM_FLAG_PRIVATE;

        VMSpace& vs = curthread->t_process.p_vmspace;
        addr_t dest_addr = reinterpret_cast<addr_t>(vo->vo_addr);
        if ((vo->vo_flags & VMOP_FLAG_FIXED) == 0)
            dest_addr = vs.ReserveAdressRange(vo->vo_len);

        VMArea* va;
        if (vo->vo_flags & VMOP_FLAG_FD) {
            if (auto result = sys_vmop_map_fd(curthread, vo, vs, vm_flags, dest_addr, va);
                result.IsFailure())
                return result;
        } else {
            if (auto result = vs.MapTo(dest_addr, vo->vo_len, vm_flags, va); result.IsFailure())
                return result;
        }

        vo->vo_addr = (void*)va->va_virt;
        vo->vo_len = va->va_len;
        return Result::Success();
    }

    Result sys_vmop_unmap(Thread* t, struct VMOP_OPTIONS* vo)
    {
        /* XXX implement me */
        return Result::Failure(EINVAL);
    }

} // namespace

Result sys_vmop(Thread* t, struct VMOP_OPTIONS* opts)
{
    /* Obtain options */
    struct VMOP_OPTIONS* vmop_opts;
    if (auto result = syscall_map_buffer(
            *t, opts, sizeof(*vmop_opts), VM_FLAG_READ | VM_FLAG_WRITE, (void**)&vmop_opts);
        result.IsFailure())
        return result;
    if (vmop_opts->vo_size != sizeof(*vmop_opts))
        return Result::Failure(EINVAL);

    switch (vmop_opts->vo_op) {
        case OP_MAP:
            return sys_vmop_map(t, vmop_opts);
        case OP_UNMAP:
            return sys_vmop_unmap(t, vmop_opts);
        default:
            return Result::Failure(EINVAL);
    }

    /* NOTREACHED */
}
