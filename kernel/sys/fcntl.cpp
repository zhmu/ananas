#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/flags.h>
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "syscall.h"

TRACE_SETUP;

Result
sys_fcntl(Thread* t, handleindex_t hindex, int cmd, const void* in, void* out)
{
	TRACE(SYSCALL, FUNC, "t=%p, hindex=%d cmd=%d", t, hindex, cmd);
	Process& process = *t->t_process;

	/* Get the handle */
	struct HANDLE* h;
	RESULT_PROPAGATE_FAILURE(
		syscall_get_handle(*t, hindex, &h)
	);

	if (h->h_type != HANDLE_TYPE_FILE)
		return RESULT_MAKE_FAILURE(EBADF);

	switch(cmd) {
		case F_DUPFD: {
			int min_fd = (int)(uintptr_t)out;
			struct HANDLE* handle_out;
			handleindex_t hidx_out;
			RESULT_PROPAGATE_FAILURE(
				handle_clone(process, hindex, NULL, process, &handle_out, min_fd, &hidx_out)
			);
			*(int*)out = hidx_out;
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
