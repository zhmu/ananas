#include <ananas/types.h>
#include <ananas/handle-options.h>
#include <ananas/error.h>
#include <ananas/stat.h>
#include <ananas/syscalls.h>
#include <elf.h>
#include "kernel/bio.h"
#include "kernel/device.h"
#include "kernel/handle.h"
#include "kernel/lib.h"
#include "kernel/syscall.h"
#include "kernel/trace.h"
#include "kernel/vm.h"

TRACE_SETUP;

static errorcode_t
sys_handlectl_generic(thread_t* t, handleindex_t hindex, struct HANDLE* h, unsigned int op, void* arg, size_t len)
{
	errorcode_t err;

	switch(op) {
		case HCTL_GENERIC_STATUS: {
			/* Ensure we understand the arguments */
			struct HCTL_STATUS_ARG* sa = arg;
			if (arg == NULL)
				return ANANAS_ERROR(BAD_ADDRESS);
			if (len != sizeof(*sa))
				return ANANAS_ERROR(BAD_LENGTH);
			sa->sa_flags = 0;
			err = ananas_success();
			break;
		}
		default:
			/* What's this? */
			err = ANANAS_ERROR(BAD_SYSCALL);
			break;
	}
	return err;
}

errorcode_t
sys_handlectl(thread_t* t, handleindex_t hindex, unsigned int op, void* arg, size_t len)
{
	TRACE(SYSCALL, FUNC, "t=%p, hindex=%u, op=%u, arg=%p, len=0x%x", t, hindex, op, arg, len);
	errorcode_t err;

	/* Fetch the handle */
	struct HANDLE* h;
	err = syscall_get_handle(t, hindex, &h);
	ANANAS_ERROR_RETURN(err);

	/* Lock the handle; we don't want it leaving our sight */
	mutex_lock(&h->h_mutex);

	/* Obtain arguments (note that some calls do not have an argument) */
	void* handlectl_arg = NULL;
	if (arg != NULL) {
		err = syscall_map_buffer(t, arg, len, VM_FLAG_READ | VM_FLAG_WRITE, &handlectl_arg);
		if(ananas_is_failure(err))
			goto fail;
	}

	/* Handle generics here; handles aren't allowed to override them */
	if (op >= _HCTL_GENERIC_FIRST && op <= _HCTL_GENERIC_LAST) {
		err = sys_handlectl_generic(t, hindex, h, op, handlectl_arg, len);
	} else {
		/* Let the handle deal with this request */
		if (h->h_hops->hop_control != NULL)
			err = h->h_hops->hop_control(t, hindex, h, op, arg, len);
		else
			err = ANANAS_ERROR(BAD_SYSCALL);
	}

fail:
	mutex_unlock(&h->h_mutex);
	return err;
}

/* vim:set ts=2 sw=2: */
