#include <ananas/types.h>
#include <ananas/dirent.h>
#include <ananas/statuscode.h>
#include <ananas/syscalls.h>
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
			statuscode_t status = sys_read(dirp->d_fd, dirp->d_buffer, dirp->d_buf_size);
			if (ananas_statuscode_is_failure(status)) {
				errno = ananas_statuscode_extract_errno(status);
				return NULL;
			}
			dirp->d_buf_filled = ananas_statuscode_extract_value(status);

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
		dirp->d_entry.d_ino = de->de_inum;
		strcpy(dirp->d_entry.d_name, de->de_name);
		return &dirp->d_entry;
	}

	/* NOTREACHED */
}

/* vim:set ts=2 sw=2: */
