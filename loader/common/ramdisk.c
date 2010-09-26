#include <ananas/types.h>
#include <loader/lib.h>
#include <loader/ramdisk.h>
#include <loader/vfs.h>
#include <cramfs.h>

#ifdef RAMDISK

#define DEBUG_RAMDISK

#ifdef DEBUG_RAMDISK
# define RAMDISK_ABORT(x...) \
	do { \
		printf(x); \
		return 0; \
	} while (0)
#else
# define RAMDISK_ABORT(x...) \
	return 0;
#endif

static int
cramfs_load(struct LOADER_RAMDISK_INFO* ram_info)
{
	struct CRAMFS_SUPERBLOCK cramfs_sb;
	if (vfs_pread(&cramfs_sb, sizeof(cramfs_sb), 0) != sizeof(cramfs_sb))
		RAMDISK_ABORT("header read error");

	if (cramfs_sb.c_magic != CRAMFS_MAGIC)
		RAMDISK_ABORT("not a cramfs disk");

	if ((cramfs_sb.c_flags & ~CRAMFS_FLAG_MASK) != 0)
		RAMDISK_ABORT("unsupported flags");

	/* Image checks out, we need to read it in full */
	uint32_t offset = 0;
	char* ptr = (char*)ram_info->ram_start;
	ram_info->ram_size = 0;
	while(1) {
		size_t len = vfs_pread(ptr, 1024, offset);
		if (len == 0)
			break;
		ram_info->ram_size += len;
		ptr += len; offset += len;
	}
	return 1;
}

int
ramdisk_load(struct LOADER_RAMDISK_INFO* ram_info)
{
	return cramfs_load(ram_info);
}

#endif /* RAMDISK */

/* vim:set ts=2 sw=2: */
