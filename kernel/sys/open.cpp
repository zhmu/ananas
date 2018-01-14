#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/flags.h>
#include "kernel/thread.h"
#include "kernel/trace.h"

TRACE_SETUP;

errorcode_t
sys_open(Thread* t, const char* path, int flags, int mode, handleindex_t* out)
{
	TRACE(SYSCALL, FUNC, "t=%p, path='%s', flags=%d, mode=%o", t, path, flags, mode);
	errorcode_t err;
	process_t* proc = t->t_process;

	/* Obtain a new handle */
	struct HANDLE* handle_out;
	handleindex_t index_out;
	err = handle_alloc(HANDLE_TYPE_FILE, proc, 0, &handle_out, &index_out);
	ANANAS_ERROR_RETURN(err);

	/*
	 * Ask the handle to open the resource - if there isn't an open operation, we
	 * assume this handle type cannot be opened using a syscall.
	 */
	if (ananas_is_success(err)) {
		if (handle_out->h_hops->hop_open != NULL)
			err = handle_out->h_hops->hop_open(t, index_out, handle_out, path, flags, mode);
		else
			err = ANANAS_ERROR(BAD_OPERATION);
	}

	if (ananas_is_failure(err)) {
		/* Open failed - destroy the handle */
		handle_free_byindex(proc, index_out);
		return err;
	}
	*out = index_out;
	TRACE(SYSCALL, FUNC, "t=%p, success, hindex=%u", t, index_out);
	return err;
}
