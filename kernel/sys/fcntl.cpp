#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/flags.h>
#include "kernel/fd.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "syscall.h"

TRACE_SETUP;

Result
sys_fcntl(Thread* t, fdindex_t hindex, int cmd, const void* in, void* out)
{
	TRACE(SYSCALL, FUNC, "t=%p, hindex=%d cmd=%d", t, hindex, cmd);
	Process& process = *t->t_process;

	/* Get the handle */
	FD* fd;
	RESULT_PROPAGATE_FAILURE(
		syscall_get_fd(*t, FD_TYPE_ANY, hindex, fd)
	);

	switch(cmd) {
		case F_DUPFD: {
			int min_fd = (int)(uintptr_t)in;
			FD* fd_out;
			fdindex_t idx_out = -1;
			RESULT_PROPAGATE_FAILURE(
				fd::Clone(process, hindex, nullptr, process, fd_out, min_fd, idx_out)
			);
			*(int*)out = idx_out;
			break;
		}
		case F_GETFD:
			/* TODO */
			*(int*)out = 0;
			break;
		case F_GETFL:
			/* TODO */
			*(int*)out = 0;
			break;
		case F_SETFD: {
			int fd = (int)(uintptr_t)out;
			/* TODO */
			(void)fd;
			break;
		}
		case F_SETFL: {
			int fl = (int)(uintptr_t)out;
			/* TODO */
			(void)fl;
			break;
		}
		default:
			return RESULT_MAKE_FAILURE(EINVAL);
	}

	return Result::Success();
}
