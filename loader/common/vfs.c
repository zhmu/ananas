#include <loader/platform.h>
#include <loader/vfs.h>
#include <stdio.h>

#define VFS_SCRATCHPAD_SIZE 1024

extern struct LOADER_FS_DRIVER loaderfs_ext2;
extern struct LOADER_FS_DRIVER loaderfs_pxe_tftp;

struct VFS_FILESYSTEMS {
	const char* name;
	struct LOADER_FS_DRIVER* driver;
} vfs_filesystems[] = {
#ifdef EXT2
	{ "ext2", &loaderfs_ext2 },
#endif
#ifdef PXE
	{ "pxe-tftp", &loaderfs_pxe_tftp },
#endif
	{ NULL, NULL }
};

void* vfs_scratchpad;
static struct LOADER_FS_DRIVER* vfs_current = NULL;
uint32_t vfs_curfile_offset;
uint32_t vfs_curfile_length;

void
vfs_init()
{
	vfs_scratchpad = platform_get_memory(VFS_SCRATCHPAD_SIZE);

	printf(">> Supporting following filesystems:");
	
	int num = 0;
	for (struct VFS_FILESYSTEMS* fs = vfs_filesystems; fs->name != NULL; fs++) {
		printf(" %s", fs->name);
		num++;
	}
	printf("%s", num ? "\n" : "(none)\n");
}

int
vfs_mount(int iodevice, const char** type)
{
	for (struct VFS_FILESYSTEMS* fs = vfs_filesystems; fs->name != NULL; fs++) {
		if (fs->driver->mount(iodevice)) {
			if (type != NULL) *type = fs->name;
			vfs_current = fs->driver;
			return 1;
		}
	}
	return 0;
}

/* These are just simple wrappers */
int
vfs_open(const char* fname)
{
	if (vfs_current == NULL)
		return 0;
	return vfs_current->open(fname);
}

void
vfs_close()
{
	if (vfs_current == NULL || vfs_current->close == NULL)
		return 0;
	vfs_current->close();
}

size_t
vfs_read(void* buffer, size_t len) {
	if (vfs_current == NULL)
		return 0;
	return vfs_current->read(buffer, len);
}

size_t
vfs_pread(void* buffer, size_t len, uint32_t offset)
{
	if (vfs_current == NULL)
		return 0;
	if (vfs_current->seek != NULL) {
		if (!vfs_current->seek(offset))
			return 0;
	} else {
		if (offset >= vfs_curfile_length)
			return 0;
		vfs_curfile_offset = offset;
	}
	return vfs_read(buffer, len);
}

const char*
vfs_readdir()
{
	if (vfs_current == NULL)
		return NULL;
 return vfs_current->readdir();
}

/* vim:set ts=2 sw=2: */
