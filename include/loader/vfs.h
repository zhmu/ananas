#include <sys/types.h>

#ifndef __LOADER_VFS_H__
#define __LOADER_VFS_H__

/*
 * This is a loader filesystem driver. It uses an extremely simple
 * single-file/single-mounted-filesystem approach to meet its intended
 * goal: to load a Ananas kernel with associated modules and get it going.
 */
struct LOADER_FS_DRIVER {
	/*
	 * Mounts a device as a filesystem.
	 */
	int (*mount)(int iodevice);

	/*
	 * Opens a file on the filesystem.
	 */
	int (*open)(const char* name);

	/*
	 * Reads a file.
	 */
	size_t (*read)(void* ptr, size_t length);

	/*
	 * Reads a directory entry.
	 */
	const char* (*readdir)();
};

/* Scratchpad data available */
#define VFS_SCRATCHPAD_LENGTH 1024

/* Scratchpad memory provided for the filesystem */
extern void* vfs_scratchpad;

/* Currently opened file offset / length */
extern uint32_t vfs_curfile_offset;
extern uint32_t vfs_curfile_length;

void vfs_init();

int vfs_mount(int iodevice, const char** type);
int vfs_open(const char* fname);
size_t vfs_read(void* buffer, size_t len);
size_t vfs_pread(void* buffer, size_t len, uint32_t offset);
uint32_t vfs_get_length();
const char* vfs_readdir();

#endif /* __LOADER_VFS_H__ */
