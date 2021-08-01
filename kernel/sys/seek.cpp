/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/handle-options.h>
#include "kernel/fd.h"
#include "kernel/result.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "syscall.h"

Result sys_seek(const fdindex_t hindex, off_t* offset, const int whence)
{
    FD* fd;
    if (auto result = syscall_get_fd(FD_TYPE_FILE, hindex, fd); result.IsFailure())
        return result;

    struct VFS_FILE* file = &fd->fd_data.d_vfs_file;
    if (file->f_dentry == NULL)
        return Result::Failure(EINVAL); /* XXX maybe re-think this for devices */

    /* Update the offset */
    off_t new_offset;
    switch (whence) {
        case HCTL_SEEK_WHENCE_SET:
            new_offset = *offset;
            break;
        case HCTL_SEEK_WHENCE_CUR:
            new_offset = file->f_offset + *offset;
            break;
        case HCTL_SEEK_WHENCE_END:
            new_offset = file->f_dentry->d_inode->i_sb.st_size - *offset;
            break;
        default:
            return Result::Failure(EINVAL);
    }
    if (new_offset < 0)
        return Result::Failure(ERANGE);
    if (new_offset > file->f_dentry->d_inode->i_sb.st_size) {
        /* File needs to be grown to accommodate for this offset */
        if (auto result = vfs_grow(file, new_offset); result.IsFailure())
            return result;
    }
    file->f_offset = new_offset;
    *offset = new_offset;
    return Result::Success();
}
