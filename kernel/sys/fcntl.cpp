#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/flags.h>
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "syscall.h"

TRACE_SETUP;

errorcode_t
sys_fcntl(Thread* t, handleindex_t hindex, int cmd, const void* in, void* out)
{
	TRACE(SYSCALL, FUNC, "t=%p, hindex=%d cmd=%d", t, hindex, cmd);
	process_t* process = t->t_process;

	/* Get the handle */
	struct HANDLE* h;
	errorcode_t err = syscall_get_handle(*t, hindex, &h);
	ANANAS_ERROR_RETURN(err);

	if (h->h_type != HANDLE_TYPE_FILE)
		return ANANAS_ERROR(BAD_HANDLE);

	switch(cmd) {
		case F_DUPFD: {
			int min_fd = (int)(uintptr_t)out;
			struct HANDLE* handle_out;
			handleindex_t hidx_out;
			err = handle_clone(process, hindex, NULL, process, &handle_out, min_fd, &hidx_out);
			ANANAS_ERROR_RETURN(err);
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
			return ANANAS_ERROR(BAD_OPERATION);
	}

	return ananas_success();
}
