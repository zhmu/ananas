#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <ananas/vfs/dirent.h>
#include <_posix/error.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>

struct dirent*
readdir(DIR* dirp)
{
	while (1) {
		if (dirp->d_cur_pos >= dirp->d_buf_filled) {
			/* Must re-read */
			if (dirp->d_flags & DE_FLAG_DONE) {
				/* Everything has been read */
				return NULL;
			}
			dirp->d_cur_pos = 0;
		}

		if (dirp->d_cur_pos == 0) {
			/* Buffer is empty; must re-read */
			size_t len = dirp->d_buf_size;
			errorcode_t err = sys_read(dirp->d_fd, dirp->d_buffer, &len);
			if (err != ANANAS_ERROR_NONE) {
				_posix_map_error(err);
				return NULL;
			}
			dirp->d_buf_filled = len;

			if (dirp->d_buf_filled == 0) {
				/* out of entries */
				dirp->d_flags |= DE_FLAG_DONE;
				dirp->d_cur_pos = dirp->d_buf_filled;
				return NULL;
			} 
		}

		struct VFS_DIRENT* de = (struct VFS_DIRENT*)(dirp->d_buffer + dirp->d_cur_pos);
		dirp->d_cur_pos += DE_LENGTH(de);
		if (de->de_name_length == 0)
			continue;
		dirp->d_entry.d_ino = 0; /* XXX not supported yet */
		strcpy(dirp->d_entry.d_name, DE_NAME(de));
		return &dirp->d_entry;
	}

	/* NOTREACHED */
}

/* vim:set ts=2 sw=2: */
