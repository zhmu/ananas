#ifndef __ENTRY_H__
#define __ENTRY_H__

#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/fs/sysfs.h>

struct SYSFS_ENTRY;
struct VFS_FILE;

#define SYSFS_ROOTINODE_NUM 1

void sysfs_init_structs();
struct SYSFS_ENTRY* sysfs_find_entry(void* fsop);

#endif /* __ENTRY_H__ */
