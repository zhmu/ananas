#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/flags.h>
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"

TRACE_SETUP;

Result
sys_open(Thread* t, const char* path, int flags, int mode, handleindex_t* out)
{
	TRACE(SYSCALL, FUNC, "t=%p, path='%s', flags=%d, mode=%o", t, path, flags, mode);
	Process& proc = *t->t_process;

	/* Obtain a new handle */
	struct HANDLE* handle_out;
	handleindex_t index_out;
	RESULT_PROPAGATE_FAILURE(
		handle_alloc(HANDLE_TYPE_FILE, proc, 0, &handle_out, &index_out)
	);

	/*
	 * Ask the handle to open the resource - if there isn't an open operation, we
	 * assume this handle type cannot be opened using a syscall.
	 */
	Result result = RESULT_MAKE_FAILURE(EINVAL);
	if (handle_out->h_hops->hop_open != NULL)
		result = handle_out->h_hops->hop_open(t, index_out, handle_out, path, flags, mode);

	if (result.IsFailure()) {
		/* Open failed - destroy the handle */
		handle_free_byindex(proc, index_out);
		return result;
	}
	*out = index_out;
	TRACE(SYSCALL, FUNC, "t=%p, success, hindex=%u", t, index_out);
	return result;
}
