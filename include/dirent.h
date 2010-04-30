#include <sys/vfs.h>

#ifndef __DIRENT_H__
#define __DIRENT_H__

#define DIRENT_BUFFER_SIZE	4096 /* XXX */

#define DE_FLAG_DONE		0x0001	/* Everything has been read */

struct dirent {
	union {
		struct VFS_DIRENT vfs_dirent;
	} u;
#define d_ino	u.vfs_dirent.inum
#define d_name	u.vfs_dirent.name
#define d_type	0
};

typedef struct {
	int	d_fd;		/* file descriptor used */
	int	d_flags;	/* flags */
	void*	d_buffer;	/* buffer for temporary storage */
	unsigned int d_cur_pos;	/* Current position */
	unsigned int d_buf_size;	/* Buffer size */
} DIR;

int closedir(DIR* dirp);
int dirfd(DIR* dirp);
DIR* opendir(const char* filename);
struct dirent* readdir(DIR* dirp);
void rewinddir(DIR* dirp);
void seekdir(DIR* dirp, long loc);
long telldir(DIR* dirp);

#endif /* __DIRENT_H__ */
