#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <_posix/handlemap.h>
#include <_posix/error.h>
#include <errno.h>
#include <unistd.h>

int close(int fd)
{
	void* handle = handlemap_deref(fd, HANDLEMAP_TYPE_ANY);
	if (handle == NULL) {
		errno = EBADF;
		return -1;
	}

	struct HANDLEMAP_OPS* hops = handlemap_get_ops(fd);
	if (hops != NULL && hops->hop_close != NULL) {
		/* Do not bother checking for errors XXX Does this make sense? */
		hops->hop_close(fd, handle);
	}

	sys_destroy(handle);
	handlemap_free_entry(fd);
	return 0;
}
