#include <ananas/types.h>
#include <loader/lib.h>
#include <loader/ramdisk.h>
#include <loader/diskio.h>
#include <loader/module.h>
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
cramfs_load(struct LOADER_MODULE* mod)
{
	addr_t dest = mod_kernel.mod_phys_end_addr;
	uint32_t offs = 0;
	while(1) {
		size_t len = vfs_pread((void*)dest, 1024, offs);
		if (len == 0)
			break;

		if (offs == 0) {
			/* We just read the first block; flush it and see if it's a cramfs disk */
			if (!diskio_flush_bulk())
				return 0;
			struct CRAMFS_SUPERBLOCK* cramfs_sb = (struct CRAMFS_SUPERBLOCK*)dest;
			if (cramfs_sb->c_magic != CRAMFS_MAGIC)
				RAMDISK_ABORT("not a cramfs disk");

			if ((cramfs_sb->c_flags & ~CRAMFS_FLAG_MASK) != 0)
				RAMDISK_ABORT("unsupported flags");
		}
		dest += len; offs += len;
	}

	mod->mod_type = MOD_RAMDISK;
	mod->mod_phys_start_addr = mod_kernel.mod_phys_end_addr;
	mod->mod_phys_end_addr = dest;
	mod_kernel.mod_phys_end_addr = dest;
	return diskio_flush_bulk();
}

int
ramdisk_load()
{
	struct LOADER_MODULE mod;
	memset(&mod, 0, sizeof(mod));
	if (!cramfs_load(&mod))
		return 0;

	/* Module was successfully loaded - find a permanent place for the module info */
	struct LOADER_MODULE* mod_info = (struct LOADER_MODULE*)(addr_t)mod_kernel.mod_phys_end_addr;
	mod_kernel.mod_phys_end_addr += sizeof(struct LOADER_MODULE);
	memcpy(mod_info, &mod, sizeof(mod));

	/* Hook it to the chain */
	struct LOADER_MODULE* mod_chain = &mod_kernel;
	while (mod_chain->mod_next != NULL)
		mod_chain = mod_chain->mod_next;
	mod_chain->mod_next = mod_info;

	return 1;
}

#endif /* RAMDISK */

/* vim:set ts=2 sw=2: */
