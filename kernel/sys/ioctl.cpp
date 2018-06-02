#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/fd.h"
#include "kernel/trace.h"
#include "kernel/result.h"
#include "kernel/vm.h"
#include "syscall.h"

TRACE_SETUP;

Result
sys_ioctl(Thread* t, fdindex_t fdindex, unsigned long req, void* arg1, void* arg2, void* arg3)
{
	TRACE(SYSCALL, FUNC, "t=%p, fd=%u, req=%p", t, fdindex, req);

	FD* fd;
	RESULT_PROPAGATE_FAILURE(
		syscall_get_fd(*t, FD_TYPE_ANY, fdindex, fd)
	);

	if (fd->fd_ops->d_ioctl == nullptr)
		return RESULT_MAKE_FAILURE(EINVAL);

	void* args[] = { arg1, arg2, arg3 };
	auto result = fd->fd_ops->d_ioctl(t, fdindex, *fd, req, args);
	if (result.IsFailure())
		return result;

	TRACE(SYSCALL, FUNC, "t=%p, success (%d)", t, result.AsStatusCode());
	return result;
}
