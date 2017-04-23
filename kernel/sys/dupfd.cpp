#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <ananas/process.h>
#include <ananas/thread.h>
#include <ananas/handle-options.h>
#include <ananas/trace.h>

TRACE_SETUP;

errorcode_t
sys_dupfd(thread_t* t, handleindex_t index, int flags, handleindex_t* out)
{
	TRACE(SYSCALL, FUNC, "t=%p, index=%d, flags=%d", t, index, flags);

	process_t* process = t->t_process;
	handleindex_t new_idx = 0;
	if (flags & HANDLE_DUPFD_TO) {
		new_idx = *out;
		sys_close(t, new_idx); /* ensure it is available; not an error if this fails */
	}

	struct HANDLE* handle_out;
	handleindex_t hidx_out;
	errorcode_t err = handle_clone(process, index, NULL, process, &handle_out, new_idx, &hidx_out);
	ANANAS_ERROR_RETURN(err);

	*out = hidx_out;
	return ananas_success();
}

/* vim:set ts=2 sw=2: */
