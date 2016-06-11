#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/handle.h>
#include <ananas/handle-options.h>
#include <ananas/trace.h>
#include <ananas/thread.h>
#include <ananas/lib.h>

TRACE_SETUP;

static errorcode_t
threadhandle_get_thread(struct HANDLE* handle, thread_t** out)
{
	struct THREAD* t = handle->h_data.d_thread;
	if (t == NULL)
		return ANANAS_ERROR(BAD_HANDLE);

	*out = t;
	return ANANAS_ERROR_OK;
}

static errorcode_t
threadhandle_control(thread_t* thread, handleindex_t index, struct HANDLE* handle, unsigned int op, void* arg, size_t len)
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
threadhandle_clone(thread_t* thread_in, handleindex_t index, struct HANDLE* handle, struct CLONE_OPTIONS* opts, thread_t* thread_out, struct HANDLE** handle_out, handleindex_t* index_out)
{
	thread_t* newthread;
	errorcode_t err = thread_clone(handle->h_thread, 0, &newthread);
	ANANAS_ERROR_RETURN(err);

	/* Look up the new thread's handle */
	struct HANDLE* thread_handle;
	err = handle_lookup(newthread, newthread->t_hidx_thread, HANDLE_TYPE_THREAD, &thread_handle);
	KASSERT(err == ANANAS_ERROR_OK, "unable to look up new thread %p handle %d: %d", newthread, newthread->t_hidx_thread, err);

	/* Create a reference to the new thread's handle; we won't be owner of it */
	err = handle_create_ref(newthread, newthread->t_hidx_thread, thread_out, handle_out, index_out);
	if (err != ANANAS_ERROR_OK) {
		thread_destroy(newthread);
		return err;
	}

	TRACE(HANDLE, INFO, "newthread handle=%x index=%d", *handle_out, *index_out);
	return ANANAS_ERROR_OK;
}

static errorcode_t
threadhandle_free(thread_t* thread, struct HANDLE* handle)
{
	thread_destroy(thread);
	return ANANAS_ERROR_OK;
}

static struct HANDLE_OPS thread_hops = {
	.hop_control = threadhandle_control,
	.hop_clone = threadhandle_clone,
	.hop_free = threadhandle_free
};

struct HANDLE_TYPE thread_handle_type = {
	.ht_name = "thread",
	.ht_id = HANDLE_TYPE_THREAD,
	.ht_hops = &thread_hops
};

/* vim:set ts=2 sw=2: */
