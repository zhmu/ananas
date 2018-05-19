#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/fd.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/vm.h"
#include "syscall.h"

TRACE_SETUP;

Result
sys_write(Thread* t, fdindex_t hindex, const void* buf, size_t* len)
{
	TRACE(SYSCALL, FUNC, "t=%p, hindex=%u, buf=%p, len=%p", t, hindex, buf, len);

	FD* fd;
	RESULT_PROPAGATE_FAILURE(
		syscall_get_fd(*t, FD_TYPE_ANY, hindex, fd)
	);

	/* Fetch the size operand */
	size_t size;
	RESULT_PROPAGATE_FAILURE(
		syscall_fetch_size(*t, len, &size)
	);

	/* Attempt to map the buffer readonly */
	void* buffer;
	RESULT_PROPAGATE_FAILURE(
		syscall_map_buffer(*t, buf, size, VM_FLAG_READ, &buffer)
	);

	/* And write data from to it */
	if (fd->fd_ops->d_write != NULL) {
		RESULT_PROPAGATE_FAILURE(
			fd->fd_ops->d_write(t, hindex, *fd, buf, &size)
		);
	} else {
		return RESULT_MAKE_FAILURE(EINVAL);
	}

	/* Finally, inform the user of the length read - the read went OK */
	RESULT_PROPAGATE_FAILURE(
		syscall_set_size(*t, len, size)
	);

	TRACE(SYSCALL, FUNC, "t=%p, success: size=%u", t, size);
	return Result::Success();
}
