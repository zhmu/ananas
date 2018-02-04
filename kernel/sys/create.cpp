#include <ananas/types.h>
#include <ananas/handle-options.h>
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/vm.h"
#include "syscall.h"

TRACE_SETUP;

Result
sys_create(Thread* t, struct CREATE_OPTIONS* opts, handleindex_t* out)
{
	TRACE(SYSCALL, FUNC, "t=%p, opts=%p, out=%p", t, opts, out);
	Process& proc = *t->t_process;

	/* Obtain arguments */
	struct CREATE_OPTIONS cropts;
	void* opts_ptr;
	RESULT_PROPAGATE_FAILURE(
		syscall_map_buffer(t, opts, sizeof(cropts), VM_FLAG_READ, &opts_ptr)
	);
	memcpy(&cropts, opts_ptr, sizeof(cropts));
	if (cropts.cr_size != sizeof(cropts))
		return RESULT_MAKE_FAILURE(EINVAL);

	/* Create a new handle and hand it to the thread*/
	struct HANDLE* out_handle;
	handleindex_t out_index;
	RESULT_PROPAGATE_FAILURE(
		handle_alloc(cropts.cr_type, proc, 0, &out_handle, &out_index)
	);
	RESULT_PROPAGATE_FAILURE(
		syscall_set_handleindex(t, out, out_index)
	);

	/*
	 * Ask the handle to create it - if there isn't a create operation,
	 * we assume this handle type cannot be created using a syscall.
	 */
	Result result = RESULT_MAKE_FAILURE(EINVAL);
	if (out_handle->h_hops->hop_create != NULL)
		result = out_handle->h_hops->hop_create(t, out_index, out_handle, &cropts);

	if (result.IsFailure()) {
		/* Create failed - destroy the handle */
		handle_free_byindex(proc, out_index);
	} else {
		TRACE(SYSCALL, INFO, "success, hindex=%u", out_index);
	}
	return result;
}

#error X
