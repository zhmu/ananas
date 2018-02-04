#ifndef __ANANAS_VFS_MOUNT_H__
#define __ANANAS_VFS_MOUNT_H__

#include <ananas/types.h>

namespace Ananas {
class Device;
}

class Result;
struct VFSFileSystem;

void vfs_init_mount();
Result vfs_register_filesystem(VFSFileSystem& fs);
Result vfs_unregister_filesystem(VFSFileSystem& fs);

Result vfs_mount(const char* from, const char* to, const char* type, void* options);
Result vfs_unmount(const char* path);

void vfs_abandon_device(Ananas::Device& device);

struct VFS_MOUNTED_FS* vfs_get_rootfs();

#endif /* __ANANAS_VFS_MOUNT_H__ */
