#include <ananas/types.h>
#include <loader/lib.h>
#include <loader/ramdisk.h>
#include <loader/vfs.h>
#include <cramfs.h>

#ifdef RAMDISK

#define DEBUG_RAMDISK

#ifdef DEBUG_RAMDISK
# define RAMDISK_ABORT(x) \
	do { \
		puts(x); \
		diskio_discard_bulk(); \
		return 0; \
	} while (0)
#else
# define RAMDISK_ABORT(x) \
	do { \
		diskio_discard_bulk(); \
		return 0; \
	} while (0)
#endif

static int
cramfs_load(struct LOADER_RAMDISK_INFO* ram_info)
{
	char* ptr = (char*)ram_info->ram_start;
	ram_info->ram_size = 0;
	while(1) {
		size_t len = vfs_pread(ptr, 1024, ram_info->ram_size);
		if (len == 0)
			break;

		if (ram_info->ram_size == 0) {
			/* We just read the first block; flush it and see if it's a cramfs disk */
			if (!diskio_flush_bulk())
				return 0;
			struct CRAMFS_SUPERBLOCK* cramfs_sb = (struct CRAMFS_SUPERBLOCK*)ptr;
			if (cramfs_sb->c_magic != CRAMFS_MAGIC)
				RAMDISK_ABORT("not a cramfs disk");

			if ((cramfs_sb->c_flags & ~CRAMFS_FLAG_MASK) != 0)
				RAMDISK_ABORT("unsupported flags");
		}

		ram_info->ram_size += len;
		ptr += len;
	}

	return diskio_flush_bulk() && ram_info->ram_size > 0;
}

int
ramdisk_load(struct LOADER_RAMDISK_INFO* ram_info)
{
	return cramfs_load(ram_info);
}

#endif /* RAMDISK */

/* vim:set ts=2 sw=2: */
