#include <ananas/syscalls.h>
#include <ananas/handle-options.h>
#include "kernel/fd.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"

TRACE_SETUP;

Result
sys_dupfd(Thread* t, fdindex_t index, int flags, fdindex_t* out)
{
	TRACE(SYSCALL, FUNC, "t=%p, index=%d, flags=%d", t, index, flags);

	Process& process = *t->t_process;
	fdindex_t new_idx = 0;
	if (flags & HANDLE_DUPFD_TO) {
		new_idx = *out;
		sys_close(t, new_idx); /* ensure it is available; not an error if this fails */
	}

	FD* fd_out;
	fdindex_t index_out;
	RESULT_PROPAGATE_FAILURE(
		fd::Clone(process, index, nullptr, process, fd_out, new_idx, index_out)
	);

	*out = index_out;
	return Result::Success();
}

/* vim:set ts=2 sw=2: */
