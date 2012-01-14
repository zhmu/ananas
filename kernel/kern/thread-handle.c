#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/handle.h>
#include <ananas/handle-options.h>
#include <ananas/trace.h>
#include <ananas/thread.h>

TRACE_SETUP;

static errorcode_t
threadhandle_get_thread(struct HANDLE* handle, thread_t** out)
{
	struct THREAD* t = handle->data.thread;
	if (t == NULL)
		return ANANAS_ERROR(BAD_HANDLE);

	*out = t;
	return ANANAS_ERROR_OK;
}

static errorcode_t
threadhandle_control(thread_t* thread, struct HANDLE* handle, unsigned int op, void* arg, size_t len)
{
	thread_t* handle_thread;
	errorcode_t err = threadhandle_get_thread(handle, &handle_thread);
	ANANAS_ERROR_RETURN(err);

	/* XXX Determine if we can manipulate the handle */

	switch(op) {
		case HCTL_THREAD_SUSPEND: {
			if (THREAD_IS_SUSPENDED(handle_thread))
				return ANANAS_ERROR(BAD_OPERATION);
			thread_suspend(handle_thread);
			return ANANAS_ERROR_OK;
		}
		case HCTL_THREAD_RESUME: {
			if (!THREAD_IS_SUSPENDED(handle_thread))
				return ANANAS_ERROR(BAD_OPERATION);
			thread_resume(handle_thread);
			return ANANAS_ERROR_OK;
		}
	}

	/* What's this? */
	return ANANAS_ERROR(BAD_SYSCALL);
}

static errorcode_t
threadhandle_clone(thread_t* thread, struct HANDLE* handle, struct HANDLE** result)
{
	thread_t* newthread;
	errorcode_t err = thread_clone(handle->thread, 0, &newthread);
	ANANAS_ERROR_RETURN(err);

	TRACE(HANDLE, INFO, "newthread handle = %x", newthread->t_thread_handle);
	*result = newthread->t_thread_handle;
	return ANANAS_ERROR_OK;
}

static struct HANDLE_OPS thread_hops = {
	.hop_control = threadhandle_control,
	.hop_clone = threadhandle_clone
};

struct HANDLE_TYPE thread_handle_type = {
	.ht_name = "thread",
	.ht_id = HANDLE_TYPE_THREAD,
	.ht_hops = &thread_hops
};

/* vim:set ts=2 sw=2: */
