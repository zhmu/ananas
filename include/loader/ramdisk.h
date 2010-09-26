#ifndef __LOADER_RAMDISK_H__
#define __LOADER_RAMDISK_H__

#include <ananas/types.h>

struct LOADER_RAMDISK_INFO {
	addr_t ram_start;
	size_t ram_size;
};

int ramdisk_load(struct LOADER_RAMDISK_INFO* ram_info);

#endif /* __LOADER_RAMDISK_H__ */
