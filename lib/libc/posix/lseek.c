#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <_posix/error.h>
#include <_posix/handlemap.h>
#include <errno.h>
#include <unistd.h>

off_t
lseek(int fd, off_t offset, int whence)
{
	void* handle = handlemap_deref(fd, HANDLEMAP_TYPE_ANY);
	struct HANDLEMAP_OPS* hops = handlemap_get_ops(fd);
	if (handle == NULL || hops == NULL || hops->hop_seek == NULL) {
		errno = EBADF; /* XXX is this correct? */
		return -1;
	}

	return hops->hop_seek(fd, handle, offset, whence);
}
