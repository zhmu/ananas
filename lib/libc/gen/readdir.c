#include <sys/types.h>
#include <dirent.h>
#include <syscalls.h>

struct dirent*
readdir(DIR* dirp)
{
	while (1) {
		if (dirp->d_cur_pos >= dirp->d_buf_size) {
			/* Must re-read */
			if (dirp->d_flags & DE_FLAG_DONE) {
				/* Everything has been read */
				return NULL;
			}
			dirp->d_cur_pos = 0;
		}

		if (dirp->d_cur_pos == 0) {
			/* Buffer is empty; must re-read */
			int num_read = sys_getdirents(dirp->d_fd, dirp->d_buffer, dirp->d_buf_size / sizeof(struct VFS_DIRENT));
			if (num_read < 0) {
				/* could not read */
				/* errno = ? */
				return NULL;
			}
			if (num_read == 0) {
				/* out of entries */
				dirp->d_flags |= DE_FLAG_DONE;
				dirp->d_cur_pos = dirp->d_buf_size;
				return NULL;
			} 
		}

		struct dirent* de = (struct dirent*)(dirp->d_buffer + dirp->d_cur_pos);
		dirp->d_cur_pos += sizeof(struct VFS_DIRENT);
		if (de->d_ino == 0)
			continue;
		return de;
	}

	/* NOTREACHED */
}

/* vim:set ts=2 sw=2: */
