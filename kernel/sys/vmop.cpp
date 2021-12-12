/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/errno.h>
#include <ananas/syscall-vmops.h>
#include "kernel/fd.h"
#include "kernel/device.h"
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/syscalls.h"
#include "kernel/thread.h"
#include "kernel/vm.h"
#include "kernel/vmarea.h"
#include "kernel/vmspace.h"
#include "kernel/vfs/dentry.h"
#include "syscall.h"

namespace
{
    Result sys_vmop_map_fd(
        struct VMOP_OPTIONS* vo, VMSpace& vs, int vm_flags, addr_t dest_addr,
        VMArea*& va)
    {
        FD* fd;
        if (auto result = syscall_get_fd(FD_TYPE_FILE, vo->vo_fd, fd);
            result.IsFailure()) {
            return result;
        }
        if (auto device = fd->fd_data.d_vfs_file.f_device; device) {
            if (device == nullptr)
                return Result::Failure(EINVAL);
            if ((vm_flags & vm::flag::Execute) != 0)
                return Result::Failure(EPERM);
            size_t length = vo->vo_len;
            addr_t physAddress;
            if (auto result = device->GetDeviceOperations().DetermineDevicePhysicalAddres(
                    physAddress, length, vm_flags);
                result.IsFailure())
                return result;
            return vs.Map(VAInterval{ dest_addr, dest_addr + length }, physAddress, vm_flags, vm_flags, va);
        }

        if (auto dentry = fd->fd_data.d_vfs_file.f_dentry; dentry) {
            const DEntryInterval deInterval{
                static_cast<off_t>(vo->vo_offset),
                static_cast<off_t>(vo->vo_offset + vo->vo_len)
            };
            return vs.MapToDentry(
                VAInterval{ dest_addr, dest_addr + vo->vo_len },
                *dentry, deInterval, vm_flags, va);
        }

        return Result::Failure(EINVAL);
    }

    Result sys_vmop_map(struct VMOP_OPTIONS* vo)
    {
        if (vo->vo_len == 0)
            return Result::Failure(EINVAL);
        if ((vo->vo_flags & (VMOP_FLAG_PRIVATE | VMOP_FLAG_SHARED)) == 0)
            return Result::Failure(EINVAL);

        int vm_flags = vm::flag::User;
        if (vo->vo_flags & VMOP_FLAG_READ)
            vm_flags |= vm::flag::Read;
        if (vo->vo_flags & VMOP_FLAG_WRITE)
            vm_flags |= vm::flag::Write;
        if (vo->vo_flags & VMOP_FLAG_EXECUTE)
            vm_flags |= vm::flag::Execute;
        if (vo->vo_flags & VMOP_FLAG_PRIVATE)
            vm_flags |= vm::flag::Private;

        VMSpace& vs = process::GetCurrent().p_vmspace;
        addr_t dest_addr = reinterpret_cast<addr_t>(vo->vo_addr);
        if ((vo->vo_flags & VMOP_FLAG_FIXED) == 0)
            dest_addr = vs.ReserveAdressRange(vo->vo_len);

        VMArea* va;
        if (vo->vo_flags & VMOP_FLAG_FD) {
            if (auto result = sys_vmop_map_fd(vo, vs, vm_flags, dest_addr, va);
                result.IsFailure())
                return result;
        } else {
            if (auto result = vs.MapTo(VAInterval{ dest_addr, dest_addr + vo->vo_len }, vm_flags, va); result.IsFailure())
                return result;
        }

        vo->vo_addr = (void*)va->va_virt;
        vo->vo_len = va->va_len;
        return Result::Success();
    }

    Result sys_vmop_unmap(struct VMOP_OPTIONS* vo)
    {
        /* XXX implement me */
        return Result::Failure(EINVAL);
    }

} // namespace

Result sys_vmop(struct VMOP_OPTIONS* opts)
{
    /* Obtain options */
    struct VMOP_OPTIONS* vmop_opts;
    if (auto result = syscall_map_buffer(
            opts, sizeof(*vmop_opts), vm::flag::Read | vm::flag::Write, (void**)&vmop_opts);
        result.IsFailure())
        return result;
    if (vmop_opts->vo_size != sizeof(*vmop_opts))
        return Result::Failure(EINVAL);

    switch (vmop_opts->vo_op) {
        case OP_MAP:
            return sys_vmop_map(vmop_opts);
        case OP_UNMAP:
            return sys_vmop_unmap(vmop_opts);
        default:
            return Result::Failure(EINVAL);
    }

    /* NOTREACHED */
}
