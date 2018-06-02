#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/fd.h"
#include "kernel/trace.h"
#include "kernel/result.h"
#include "kernel/vm.h"
#include "syscall.h"

TRACE_SETUP;

Result
sys_read(Thread* t, fdindex_t hindex, void* buf, size_t len)
{
	TRACE(SYSCALL, FUNC, "t=%p, hindex=%u, buf=%p, len=%d", t, hindex, buf, len);

	FD* fd;
	RESULT_PROPAGATE_FAILURE(
		syscall_get_fd(*t, FD_TYPE_ANY, hindex, fd)
	);

	// Attempt to map the buffer write-only
	void* buffer;
	RESULT_PROPAGATE_FAILURE(
		syscall_map_buffer(*t, buf, len, VM_FLAG_WRITE, &buffer)
	);

	// And read data to it
	if (fd->fd_ops->d_read == nullptr)
		return RESULT_MAKE_FAILURE(EINVAL);

	auto result = fd->fd_ops->d_read(t, hindex, *fd, buf, len);
	if (result.IsFailure())
		return result;

	TRACE(SYSCALL, FUNC, "t=%p, success: size %d", t, result.AsStatusCode());
	return result;
}
