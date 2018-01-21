#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/handle-options.h>
#include "kernel/lib.h"
#include "kernel/trace.h"
#include "kernel/vm.h"
#include "syscall.h"

TRACE_SETUP;

errorcode_t
sys_create(Thread* t, struct CREATE_OPTIONS* opts, handleindex_t* out)
{
	TRACE(SYSCALL, FUNC, "t=%p, opts=%p, out=%p", t, opts, out);
	errorcode_t err;
	Process& proc = *t->t_process;

	/* Obtain arguments */
	struct CREATE_OPTIONS cropts;
	void* opts_ptr;
	err = syscall_map_buffer(t, opts, sizeof(cropts), VM_FLAG_READ, &opts_ptr);
	ANANAS_ERROR_RETURN(err);
	memcpy(&cropts, opts_ptr, sizeof(cropts));
	if (cropts.cr_size != sizeof(cropts))
		return ANANAS_ERROR(BAD_LENGTH);

	/* Create a new handle and hand it to the thread*/
	struct HANDLE* out_handle;
	handleindex_t out_index;
	err = handle_alloc(cropts.cr_type, proc, 0, &out_handle, &out_index);
	ANANAS_ERROR_RETURN(err);
	err = syscall_set_handleindex(t, out, out_index);
	ANANAS_ERROR_RETURN(err);

	/*
	 * Ask the handle to create it - if there isn't a create operation,
	 * we assume this handle type cannot be created using a syscall.
	 */
	if (out_handle->h_hops->hop_create != NULL)
		err = out_handle->h_hops->hop_create(t, out_index, out_handle, &cropts);
	else
		err = ANANAS_ERROR(BAD_OPERATION);

	if (ananas_is_failure(err)) {
		/* Create failed - destroy the handle */
		handle_free_byindex(proc, out_index);
	} else {
		TRACE(SYSCALL, INFO, "success, hindex=%u", out_index);
	}
	return err;
}

#error X
