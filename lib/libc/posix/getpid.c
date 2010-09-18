#include <unistd.h>
#include <ananas/threadinfo.h>

pid_t
getpid()
{
	return (pid_t)libc_threadinfo->ti_handle;
}
