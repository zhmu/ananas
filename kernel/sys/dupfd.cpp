#include <ananas/syscalls.h>
#include <ananas/handle-options.h>
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"

TRACE_SETUP;

Result
sys_dupfd(Thread* t, handleindex_t index, int flags, handleindex_t* out)
{
	TRACE(SYSCALL, FUNC, "t=%p, index=%d, flags=%d", t, index, flags);

	Process& process = *t->t_process;
	handleindex_t new_idx = 0;
	if (flags & HANDLE_DUPFD_TO) {
		new_idx = *out;
		sys_close(t, new_idx); /* ensure it is available; not an error if this fails */
	}

	struct HANDLE* handle_out;
	handleindex_t hidx_out;
	RESULT_PROPAGATE_FAILURE(
		handle_clone(process, index, NULL, process, &handle_out, new_idx, &hidx_out)
	);

	*out = hidx_out;
	return Result::Success();
}

/* vim:set ts=2 sw=2: */
