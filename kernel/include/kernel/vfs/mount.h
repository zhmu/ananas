/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __ANANAS_VFS_MOUNT_H__
#define __ANANAS_VFS_MOUNT_H__

#include <ananas/types.h>

class Device;

class Result;
struct VFSFileSystem;

Result vfs_register_filesystem(VFSFileSystem& fs);
Result vfs_unregister_filesystem(VFSFileSystem& fs);

Result vfs_mount(const char* from, const char* to, const char* type);
Result vfs_unmount(const char* path);

void vfs_abandon_device(Device& device);

struct VFS_MOUNTED_FS* vfs_get_rootfs();

#endif /* __ANANAS_VFS_MOUNT_H__ */
