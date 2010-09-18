/*
 * This is a iso9660 driver for the loader; as the loader can only read
 * single sectors, it relies on the fact that a CD sector consists of a
 * multiple of 512-byte sectors.
 */
#include <ananas/types.h>
#include <loader/diskio.h>
#include <loader/vfs.h>
#include <loader/platform.h>
#include <stdio.h>
#include <string.h>
#include <iso9660.h>

#ifdef ISO9660

#define ISO9660_BLOCK_SIZE 2048

#ifdef _BIG_ENDIAN
# define ISO9660_GET_WORD(x) (*(uint16_t*)((x)+2))
# define ISO9660_GET_DWORD(x) (*(uint32_t*)((x)+4))
#else
# define ISO9660_GET_WORD(x) (*(uint16_t*)(x))
# define ISO9660_GET_DWORD(x) (*(uint32_t*)(x))
#endif

struct ISO9660_ACTIVE_FILESYSTEM {
	int device;
	uint32_t rootentry_lba;
	uint32_t rootentry_length;
};

struct ISO9660_ACTIVE_FILE {
	uint32_t start_lba;
	uint32_t next_lba;
	uint32_t next_length;
	uint8_t current_entryname[256];
};

static struct ISO9660_ACTIVE_FILESYSTEM* iso9660_activefs;
static struct ISO9660_ACTIVE_FILE* iso9660_activefile;

static void*
iso9660_get_block(int device, int block)
{
	static void* buffer = NULL;
	if (buffer == NULL)
		buffer = platform_get_memory(ISO9660_BLOCK_SIZE);

	for (unsigned int i = 0; i < ISO9660_BLOCK_SIZE / SECTOR_SIZE; i++) {
		struct CACHE_ENTRY* centry = diskio_read(device, (block * (ISO9660_BLOCK_SIZE / SECTOR_SIZE)) + i);
		if (centry == NULL)
			return NULL;
		memcpy((void*)(buffer + (i * SECTOR_SIZE)), centry->data, SECTOR_SIZE);
	}
	return buffer;
}

static int
iso9660_mount(int device)
{
	struct ISO9660_PRIMARY_VOLUME_DESCR* pvd = (struct ISO9660_PRIMARY_VOLUME_DESCR*)iso9660_get_block(device, 16);
	if (pvd == NULL)
		return 0;

	/* do basic iso9660 checks; if this does not match, reject the filesystem */
  if (pvd->pv_typecode != 1 || memcmp(pvd->pv_stdentry, "CD001", 5) || pvd->pv_version != 1 || pvd->pv_structure_version != 1)
		return 0;
	if (ISO9660_GET_WORD(pvd->pv_blocksize) != ISO9660_BLOCK_SIZE)
		return 0;

	/* Check the root entry. This may be overly paranoid */
	struct ISO9660_DIRECTORY_ENTRY* rootentry = (struct ISO9660_DIRECTORY_ENTRY*)pvd->pv_root_direntry;
	if (rootentry->de_length != 34 || rootentry->de_filename_len != 1 || rootentry->de_filename[0] != '\0' ||
	    (rootentry->de_flags & DE_FLAG_DIRECTORY) == 0)
		return 0;

	/* OK, all set! */
	iso9660_activefs = vfs_scratchpad;
	iso9660_activefs->device = device;
	iso9660_activefs->rootentry_lba = ISO9660_GET_DWORD(rootentry->de_extent_lba);
	iso9660_activefs->rootentry_length = ISO9660_GET_DWORD(rootentry->de_data_length);

	iso9660_activefile = (struct ISO9660_ACTIVE_FILE*)(vfs_scratchpad + sizeof(struct ISO9660_ACTIVE_FILESYSTEM));
	return 1;
}

static size_t
iso9660_read(void* buf, size_t len)
{
  /* Normalize len so that it cannot expand beyond the file size */
  if (vfs_curfile_offset + len > vfs_curfile_length)
    len = vfs_curfile_length - vfs_curfile_offset;

	size_t numread = 0;
	while (len > 0) {
		void* buffer = iso9660_get_block(iso9660_activefs->device, iso9660_activefile->start_lba + (vfs_curfile_offset / ISO9660_BLOCK_SIZE));
		if (buffer == NULL)
			break;

		int chunklen = ISO9660_BLOCK_SIZE - (vfs_curfile_offset % ISO9660_BLOCK_SIZE);
		if (chunklen > len)
			chunklen = len;
		memcpy(buf, (void*)(buffer + (vfs_curfile_offset % ISO9660_BLOCK_SIZE)), chunklen);
		len -= chunklen; buf += chunklen;

		vfs_curfile_offset += chunklen;
    numread += chunklen;
	}
	return numread;
}

static const char*
iso9660_readdir()
{
	/*
	 * ISO9660 will never store a partial entry in a block; this makes our
	 * implementation easier as we can just grab a complete block at a time.
	 */
	while(1) {
		void* buffer = iso9660_get_block(iso9660_activefs->device, iso9660_activefile->start_lba + (vfs_curfile_offset / ISO9660_BLOCK_SIZE));
		if (buffer == NULL)
			return NULL;
		struct ISO9660_DIRECTORY_ENTRY* iso9660de = (struct ISO9660_DIRECTORY_ENTRY*)(buffer + (vfs_curfile_offset % ISO9660_BLOCK_SIZE));
		vfs_curfile_offset += iso9660de->de_length;
		if ((iso9660de->de_flags & DE_FLAG_HIDDEN) != 0)
			continue;
		if (iso9660de->de_filename_len == 0)
			return NULL;

		/* Skip entries that are too short; we only care things we can display */
		if (iso9660de->de_filename_len < 2)
			continue;

		/*
		 * OK, ISO9660 likes UPPERCASE and appends useless ;1 after every entry; we just
		 * nuke these out of visual comfort.
		 */
		if (iso9660de->de_filename[iso9660de->de_filename_len - 2] == ';' &&
				iso9660de->de_filename[iso9660de->de_filename_len - 1] == '1')
			iso9660de->de_filename_len -= 2;
		for (int i = 0; i <  iso9660de->de_filename_len; i++) {
			if (iso9660de->de_filename[i] >= 'A' && iso9660de->de_filename[i] <= 'Z')
				iso9660de->de_filename[i] += 'a' - 'A';
		}

		/* If the final charachter is only a '.', kill it */
		if (iso9660de->de_filename_len > 0 && iso9660de->de_filename[iso9660de->de_filename_len - 1] == '.')
			iso9660de->de_filename_len--;


		/*
		 * Store the entry LBA/offset and the length; we need this if we open the
		 * entry later.
		 */
		iso9660_activefile->next_lba = ISO9660_GET_DWORD(iso9660de->de_extent_lba);
		iso9660_activefile->next_length = ISO9660_GET_DWORD(iso9660de->de_data_length);
		strcpy(iso9660_activefile->current_entryname, iso9660de->de_filename);
		iso9660_activefile->current_entryname[iso9660de->de_filename_len] = '\0';
		return iso9660_activefile->current_entryname;
	}
	
	return NULL;
}

static int
iso9660_open(const char* fname)
{
	/* Activate the root entry */
	iso9660_activefile->start_lba = iso9660_activefs->rootentry_lba;
	vfs_curfile_length = iso9660_activefs->rootentry_length;
	vfs_curfile_offset = 0;

	while (1) {
		while (*fname == '/')
			fname++;
		if (*fname == '\0') {
			/* all done! */
			return 1;
		}

		/* figure out how much bytes we have to check */
		char* ptr = strchr(fname, '/');
		if (ptr == NULL)
			ptr = strchr(fname, '\0');
		int len = ptr - fname;

		/* walk through the directory */
		int got = 0;
		const char* entry;
		while ((entry = iso9660_readdir()) != NULL) {
			if (strncmp(entry, fname, len) == 0) {
				iso9660_activefile->start_lba = iso9660_activefile->next_lba;
				vfs_curfile_length = iso9660_activefile->next_length;
				vfs_curfile_offset = 0;
				fname += len; got++;
				break;
			}
		}
		if (!got)
			return 0;
	}
}

struct LOADER_FS_DRIVER loaderfs_iso9660 = {
	.mount = &iso9660_mount,
	.open = &iso9660_open,
	.read = &iso9660_read,
	.readdir = &iso9660_readdir,
};

#endif /* ISO9660 */

/* vim:set ts=2 sw=2: */
