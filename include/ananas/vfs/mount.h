#ifndef __ANANAS_VFS_MOUNT_H__
#define __ANANAS_VFS_MOUNT_H__

void vfs_init_mount();

errorcode_t vfs_mount(const char* from, const char* to, const char* type, void* options);
errorcode_t vfs_unmount(const char* path);

struct VFS_MOUNTED_FS* vfs_get_rootfs();

void kdb_cmd_vfs_mounts(int num_args, char** arg);

#endif /* __ANANAS_VFS_MOUNT_H__ */
