#include <unistd.h>
#include <sys/threadinfo.h>

pid_t
getpid()
{
	return (pid_t)libc_threadinfo->ti_handle;
}
