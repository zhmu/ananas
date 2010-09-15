#include <unistd.h>
#include <sys/threadinfo.h>
#include <_posix/fdmap.h>

extern void* sys_clone(void* handle);

pid_t fork()
{
	return sys_clone(libc_threadinfo->ti_handle);
}
