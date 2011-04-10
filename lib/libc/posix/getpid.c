#include <unistd.h>
#include <ananas/threadinfo.h>
#include <_posix/handlemap.h>

pid_t
getpid()
{
	/* XXX This can be much more efficient */
	for (int idx = 0; idx < HANDLEMAP_SIZE; idx++) {
		if (libc_threadinfo->ti_handle == handlemap_deref(idx, HANDLEMAP_TYPE_PID))
			return idx;
	}
	/* ??? */
	return (pid_t)-1;
}
