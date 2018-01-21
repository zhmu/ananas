#ifndef __ANANAS_VFS_MOUNT_H__
#define __ANANAS_VFS_MOUNT_H__

#include <ananas/types.h>

namespace Ananas {
class Device;
}

struct VFSFileSystem;

void vfs_init_mount();
errorcode_t vfs_register_filesystem(VFSFileSystem& fs);
errorcode_t vfs_unregister_filesystem(VFSFileSystem& fs);

errorcode_t vfs_mount(const char* from, const char* to, const char* type, void* options);
errorcode_t vfs_unmount(const char* path);

void vfs_abandon_device(Ananas::Device& device);

struct VFS_MOUNTED_FS* vfs_get_rootfs();

#endif /* __ANANAS_VFS_MOUNT_H__ */
