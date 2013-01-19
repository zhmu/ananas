#include <machine/param.h>
#include <ananas/types.h>
#include <ananas/bio.h>
#include <ananas/device.h>
#include <ananas/lib.h>
#include <ananas/handle.h>
#include <ananas/handle-options.h>
#include <ananas/error.h>
#include <ananas/thread.h>
#include <ananas/pcpu.h>
#include <ananas/stat.h>
#include <ananas/schedule.h>
#include <ananas/syscall.h>
#include <ananas/syscalls.h>
#include <ananas/trace.h>
#include <elf.h>

TRACE_SETUP;

errorcode_t
sys_read(thread_t* t, handle_t handle, void* buf, size_t* len)
{
	TRACE(SYSCALL, FUNC, "t=%p, handle=%p, buf=%p, len=%p", t, handle, buf, len);
	errorcode_t err;

	/* Get the handle */
	struct HANDLE* h;
	err = syscall_get_handle(t, handle, &h);
	ANANAS_ERROR_RETURN(err);

	/* Fetch the size operand */
	size_t size;
	err = syscall_fetch_size(t, len, &size);
	ANANAS_ERROR_RETURN(err);

	/* Attempt to map the buffer write-only */
	void* buffer;
	err = syscall_map_buffer(t, buf, size, THREAD_MAP_WRITE, &buffer);
	ANANAS_ERROR_RETURN(err);

	/* And read data to it */
	if (h->h_hops->hop_read != NULL)
		err = h->h_hops->hop_read(t, h, buf, &size);
	else
		err = ANANAS_ERROR(BAD_OPERATION);

	/* Finally, inform the user of the length read - the read went OK */
	err = syscall_set_size(t, len, size);
	ANANAS_ERROR_RETURN(err);

	TRACE(SYSCALL, FUNC, "t=%p, success: size=%u", t, size);
	return err;
}

errorcode_t
sys_write(thread_t* t, handle_t handle, const void* buf, size_t* len)
{
	TRACE(SYSCALL, FUNC, "t=%p, handle=%p, buf=%p, len=%p", t, handle, buf, len);
	errorcode_t err;

	/* Get the handle */
	struct HANDLE* h;
	err = syscall_get_handle(t, handle, &h);
	ANANAS_ERROR_RETURN(err);

	/* Fetch the size operand */
	size_t size;
	err = syscall_fetch_size(t, len, &size);
	ANANAS_ERROR_RETURN(err);

	/* Attempt to map the buffer readonly */
	void* buffer;
	err = syscall_map_buffer(t, buf, size, THREAD_MAP_READ, &buffer);
	ANANAS_ERROR_RETURN(err);

	/* And write data from to it */
	if (h->h_hops->hop_write != NULL)
		err = h->h_hops->hop_write(t, h, buf, &size);
	else
		err = ANANAS_ERROR(BAD_OPERATION);
	ANANAS_ERROR_RETURN(err);

	/* Finally, inform the user of the length read - the read went OK */
	err = syscall_set_size(t, len, size);
	ANANAS_ERROR_RETURN(err);

	TRACE(SYSCALL, FUNC, "t=%p, success: size=%u", t, size);
	return err;
}

errorcode_t
sys_open(thread_t* t, struct OPEN_OPTIONS* opts, handle_t* result)
{
	TRACE(SYSCALL, FUNC, "t=%p, opts=%p, result=%p", t, opts, result);
	errorcode_t err;

	/* Obtain open options */
	struct OPEN_OPTIONS open_opts; void* optr;
	err = syscall_map_buffer(t, opts, sizeof(open_opts), THREAD_MAP_READ, &optr);
	ANANAS_ERROR_RETURN(err);
	memcpy(&open_opts, optr, sizeof(open_opts));
	if (open_opts.op_size != sizeof(open_opts))
		return ANANAS_ERROR(BAD_LENGTH);

	/* Fetch the path name */
	const char* userpath;
	err = syscall_map_string(t, open_opts.op_path, &userpath);
	ANANAS_ERROR_RETURN(err);
	open_opts.op_path = userpath;

	/* Obtain a new handle */
	struct HANDLE* handle;
	err = handle_alloc(open_opts.op_type, t, &handle);
	ANANAS_ERROR_RETURN(err);
	err = syscall_set_handle(t, result, handle);
	if (err != ANANAS_ERROR_OK) {
		handle_free(handle);
		return err;
	}

	/*
	 * Ask the handle to open the resource - if there isn't an open operation, we
	 * assume this handle type cannot be opened using a syscall.
	 */
	if (handle->h_hops->hop_open != NULL)
		err = handle->h_hops->hop_open(t, handle, &open_opts);
	else
		err = ANANAS_ERROR(BAD_OPERATION);

	if (err != ANANAS_ERROR_OK) {
		/* Create failed - destroy the handle */
		handle_free(handle);
		return err;
	}
	TRACE(SYSCALL, FUNC, "t=%p, success, handle=%p", t, handle);
	return err;
}

errorcode_t
sys_destroy(thread_t* t, handle_t handle)
{
	TRACE(SYSCALL, FUNC, "t=%p, handle=%p", t, handle);

	struct HANDLE* h;
	errorcode_t err = syscall_get_handle(t, handle, &h);
	ANANAS_ERROR_RETURN(err);
	err = handle_free(h);
	ANANAS_ERROR_RETURN(err);

	TRACE(SYSCALL, INFO, "t=%p, success", t);
	return err;
}

errorcode_t
sys_clone(thread_t* t, handle_t in, struct CLONE_OPTIONS* opts, handle_t* out)
{
	TRACE(SYSCALL, FUNC, "t=%p, in=%p, opts=%p, out=%p", t, in, opts, out);
	errorcode_t err;

	/* Obtain clone options */
	struct CLONE_OPTIONS clone_opts; void* optr;
	err = syscall_map_buffer(t, opts, sizeof(clone_opts), THREAD_MAP_READ, &optr);
	ANANAS_ERROR_RETURN(err);
	memcpy(&clone_opts, optr, sizeof(clone_opts));
	if (clone_opts.cl_size != sizeof(clone_opts))
		return ANANAS_ERROR(BAD_LENGTH);

	/* Get the handle */
	struct HANDLE* handle;
	err = syscall_get_handle(t, in, &handle);
	ANANAS_ERROR_RETURN(err);

	/* Try to clone it */
	struct HANDLE* dest;
	err = handle_clone(t, handle, &clone_opts, &dest);
	ANANAS_ERROR_RETURN(err);

	/*
	 * And hand the cloned handle back - note that the return code will be
	 * overriden for the new child, if we are cloning a thread handle.
	 */
	err = syscall_set_handle(t, out, dest);
	ANANAS_ERROR_RETURN(err);

	TRACE(SYSCALL, FUNC, "t=%p, success, new handle=%p", t, dest);
	return err;
}

errorcode_t
sys_wait(thread_t* t, handle_t handle, handle_event_t* event, handle_event_result_t* result)
{
	TRACE(SYSCALL, FUNC, "t=%p, handle=%p, event=%p, result=%p", t, handle, event, result);
	errorcode_t err;

	/* Fetch the handle */
	struct HANDLE* h;
	err = syscall_get_handle(t, handle, &h);
	ANANAS_ERROR_RETURN(err);

	/* Obtain the event for which to wait */
	handle_event_t wait_event = 0; /* XXX */
	if (event != NULL) {
		handle_event_t* in_event;
		err = syscall_map_buffer(t, event, sizeof(handle_event_t), THREAD_MAP_READ, (void**)&in_event);
		ANANAS_ERROR_RETURN(err);
		wait_event = *in_event;
	}

	/* Wait for the handle */
	handle_event_result_t wait_result;
	err = handle_wait(t, h, &wait_event, &wait_result);
	ANANAS_ERROR_RETURN(err);

	/* If the event type was requested, set it up */
	if (event != NULL) {
		handle_event_t* out_event;
		err = syscall_map_buffer(t, event, sizeof(handle_event_t), THREAD_MAP_WRITE, (void**)&out_event);
		ANANAS_ERROR_RETURN(err);
		*out_event = wait_event;
	}

	/* If the result was requested, set it up */
	if (result != NULL) {
		handle_event_result_t* out_result;
		err = syscall_map_buffer(t, result, sizeof(handle_event_result_t), THREAD_MAP_WRITE, (void*)&out_result);
		ANANAS_ERROR_RETURN(err);
		*out_result = wait_result;
	}

	/* All done */
	TRACE(SYSCALL, FUNC, "t=%p, success, event=0x%x, result=0x%x", t, wait_event, wait_result);
	return ANANAS_ERROR_OK;
}

errorcode_t
sys_summon(thread_t* t, handle_t handle, struct SUMMON_OPTIONS* opts, handle_t* out)
{
	TRACE(SYSCALL, FUNC, "t=%p, handle=%p, opts=%p, out=%p", t, handle, opts, out);
	errorcode_t err;

	/* Get the handle */
	struct HANDLE* h;
	err = syscall_get_handle(t, handle, &h);
	ANANAS_ERROR_RETURN(err);

	/* Obtain summoning options */
	struct SUMMON_OPTIONS summon_opts; void* so;
	err = syscall_map_buffer(t, opts, sizeof(summon_opts), THREAD_MAP_READ, &so);
	ANANAS_ERROR_RETURN(err);
	memcpy(&summon_opts, so, sizeof(summon_opts));

	/*
	 * Ask the handle to summon - if there isn't a summon operation,
	 * we assume this handle type cannot be summoned using a syscall.
	 */
	if (h->h_hops->hop_summon != NULL)
		err = h->h_hops->hop_summon(t, h, &summon_opts, (struct HANDLE**)out);
	else
		err = ANANAS_ERROR(BAD_OPERATION);

	if (err == ANANAS_ERROR_OK) {
		TRACE(SYSCALL, INFO, "success, handle=%p", out);
	}
	return err;
}

errorcode_t
sys_create(thread_t* t, struct CREATE_OPTIONS* opts, handle_t* out)
{
	TRACE(SYSCALL, FUNC, "t=%p, opts=%p, out=%p", t, opts, out);
	errorcode_t err;

	/* Obtain arguments */
	struct CREATE_OPTIONS cropts;
	void* opts_ptr;
	err = syscall_map_buffer(t, opts, sizeof(cropts), THREAD_MAP_READ, &opts_ptr);
	ANANAS_ERROR_RETURN(err);
	memcpy(&cropts, opts_ptr, sizeof(cropts));
	if (cropts.cr_size != sizeof(cropts))
		return ANANAS_ERROR(BAD_LENGTH);

	/* Create a new handle and hand it to the thread*/
	struct HANDLE* outhandle;
	err = handle_alloc(cropts.cr_type, t, &outhandle);
	ANANAS_ERROR_RETURN(err);
	err = syscall_set_handle(t, out, outhandle);
	ANANAS_ERROR_RETURN(err);

	/*
	 * Ask the handle to create it - if there isn't a create operation,
	 * we assume this handle type cannot be created using a syscall.
	 */
	if (outhandle->h_hops->hop_create != NULL)
		err = outhandle->h_hops->hop_create(t, outhandle, &cropts);
	else
		err = ANANAS_ERROR(BAD_OPERATION);

	if (err != ANANAS_ERROR_OK) {
		/* Create failed - destroy the handle */
		handle_free(outhandle);
	} else {
		TRACE(SYSCALL, INFO, "success, handle=%p", outhandle);
	}
	return err;
}

errorcode_t
sys_unlink(thread_t* t, handle_t handle)
{
	TRACE(SYSCALL, FUNC, "t=%p, handle=%p", t, handle);
	errorcode_t err;

	/* Fetch the handle */
	struct HANDLE* h;
	err = syscall_get_handle(t, handle, &h);
	ANANAS_ERROR_RETURN(err);

	kprintf("sys_unlink(): todo\n");
	return ANANAS_ERROR(BAD_SYSCALL);
}

static errorcode_t
sys_handlectl_generic(thread_t* t, handle_t handle, unsigned int op, void* arg, size_t len)
{
	errorcode_t err;

	switch(op) {
		case HCTL_GENERIC_SETOWNER: {
			void* owner = arg;
			if (len != sizeof(void*)) {
				err = ANANAS_ERROR(BAD_LENGTH);
				goto fail;
			}
			err = handle_set_owner(t, handle, owner);
			if (err == ANANAS_ERROR_OK) {
				TRACE(SYSCALL, INFO, "t=%p, handle=%p, new owner=%p, success", t, handle, owner);
			} else {
				TRACE(SYSCALL, ERROR, "t=%p, handle=%p, new owner=%p, failure, code %u", t, handle, owner, err);
			}
			break;
		}
		default:
			/* What's this? */
			err = ANANAS_ERROR(BAD_SYSCALL);
			break;
	}
fail:
	return err;
}

errorcode_t
sys_handlectl(thread_t* t, handle_t handle, unsigned int op, void* arg, size_t len)
{
	TRACE(SYSCALL, FUNC, "t=%p, handle=%p, op=%u, arg=%p, len=0x%x", t, handle, op, arg, len);
	errorcode_t err;

	/* Fetch the handle */
	struct HANDLE* h;
	err = syscall_get_handle(t, handle, &h);
	ANANAS_ERROR_RETURN(err);

	/* Lock the handle; we don't want it leaving our sight */
	mutex_lock(&h->h_mutex);

	/* Obtain arguments (note that some calls do not have an argument) */
	void* handlectl_arg = NULL;
	if (arg != NULL) {
		err = syscall_map_buffer(t, arg, len, THREAD_MAP_READ | THREAD_MAP_WRITE, &handlectl_arg);
		if(err != ANANAS_ERROR_OK)
			goto fail;
	}

	/* Handle generics here; handles aren't allowed to override them */
	if (op >= _HCTL_GENERIC_FIRST && op <= _HCTL_GENERIC_LAST) {
		err = sys_handlectl_generic(t, handle, op, handlectl_arg, len);
	} else {
		/* Let the handle deal with this request */
		if (h->h_hops->hop_control != NULL)
			err = h->h_hops->hop_control(t, handle, op, arg, len);
		else
			err = ANANAS_ERROR(BAD_SYSCALL);
	}

fail:
	mutex_unlock(&h->h_mutex);
	return err;
}

/* vim:set ts=2 sw=2: */
