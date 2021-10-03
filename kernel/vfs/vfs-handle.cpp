/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/flags.h>
#include <ananas/handle-options.h>
#include "kernel/bio.h"
#include "kernel/fd.h"
#include "kernel/init.h"
#include "kernel/lib.h"
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/device.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"

namespace
{
    Result vfshandle_get_file(FD& fd, struct VFS_FILE*& out)
    {
        if (fd.fd_type != FD_TYPE_FILE)
            return Result::Failure(EBADF);

        struct VFS_FILE* file = &fd.fd_data.d_vfs_file;
        if (file->f_dentry == nullptr && file->f_device == nullptr)
            return Result::Failure(EBADF);

        out = file;
        return Result::Success();
    }

    Result vfshandle_read(const fdindex_t index, FD& fd, void* buffer, const size_t size)
    {
        struct VFS_FILE* file;
        if (auto result = vfshandle_get_file(fd, file); result.IsFailure())
            return result;

        return vfs_read(file, buffer, size);
    }

    Result vfshandle_write(const fdindex_t index, FD& fd, const void* buffer, const size_t size)
    {
        struct VFS_FILE* file;
        if (auto result = vfshandle_get_file(fd, file); result.IsFailure())
            return result;

        return vfs_write(file, buffer, size);
    }

    Result vfshandle_open(const fdindex_t index, FD& fd, const char* path, const int flags, const int mode)
    {
        auto& proc = process::GetCurrent();

        /*
         * If we could try to create the file, do so - if this fails, we'll attempt
         * the ordinary open. This should have the advantage of eliminating a race
         * condition.
         */
        if (flags & O_CREAT) {
            /* Attempt to create the new file - if this works, we're all set */
            Result result = vfs_create(proc.p_cwd, &fd.fd_data.d_vfs_file, path, mode);
            if (result.IsSuccess())
                return result;

            /*
             * File could not be created; if we had to create the file, this is an
             * error whatsoever, otherwise we'll report anything but 'file already
             * exists' back to the caller.
             */
            if ((flags & O_EXCL) || result.AsErrno() != EEXIST)
                return result;

            /* File can't be created - try opening it instead */
        }

        /* XXX do something more with mode / open */

        /* And open the path */
        return vfs_open(&proc, path, proc.p_cwd, &fd.fd_data.d_vfs_file);
    }

    Result vfshandle_free(Process& proc, FD& fd)
    {
        auto& file = fd.fd_data.d_vfs_file;
        return vfs_close(&proc, &file);
    }

    Result vfshandle_unlink(const fdindex_t index, FD& fd)
    {
        struct VFS_FILE* file;
        if (auto result = vfshandle_get_file(fd, file); result.IsFailure())
            return result;

        return vfs_unlink(file);
    }

    Result vfshandle_clone(
        Process& p_in, fdindex_t index_in, FD& fd_in, struct CLONE_OPTIONS* opts, Process& proc_out,
        FD*& fd_out, fdindex_t index_out_min, fdindex_t& index_out)
    {
        struct VFS_FILE* file;
        if (auto result = vfshandle_get_file(fd_in, file); result.IsFailure())
            return result;

        /*
         * If the handle has a backing dentry reference, we have to increase it's
         * reference count as we, too, depend on it. Closing the handle
         * will release the dentry, which will remove it if needed.
         */
        DEntry* dentry = file->f_dentry;
        if (dentry != nullptr)
            dentry_ref(*dentry);

        /* Now, just ordinarely clone the handle */
        return fd::CloneGeneric(fd_in, proc_out, fd_out, index_out_min, index_out);
    }

    Result
    vfshandle_ioctl(fdindex_t fdindex, FD& fd, unsigned long request, void* args[])
    {
        struct VFS_FILE* file;
        if (auto result = vfshandle_get_file(fd, file); result.IsFailure())
            return result;
        auto& proc = process::GetCurrent();
        return vfs_ioctl(&proc, file, request, args);
    }

    namespace
    {
        bool vfshandle_can_read(fdindex_t, FD& fd)
        {
            struct VFS_FILE* file;
            if (auto result = vfshandle_get_file(fd, file); result.IsFailure())
                return false; // ???

            if (auto device = file->f_device; device != nullptr) {
                if (device->GetCharDeviceOperations() != nullptr)
                    return device->GetCharDeviceOperations()->CanRead();
            }

            if (file->f_dentry != nullptr)
                return true;

            return false;
        }
    }

    struct FDOperations vfs_ops = {
        .d_read = vfshandle_read,
        .d_write = vfshandle_write,
        .d_open = vfshandle_open,
        .d_free = vfshandle_free,
        .d_unlink = vfshandle_unlink,
        .d_clone = vfshandle_clone,
        .d_ioctl = vfshandle_ioctl,
        .d_can_read = vfshandle_can_read,
    };

    // TODO It would be nice if we could make this more generic
    const init::OnInit registerFDType(init::SubSystem::Handle, init::Order::Second, []() {
        static FDType ft("file", FD_TYPE_FILE, vfs_ops);
        fd::RegisterType(ft);
    });

} // unnamed namespace
