#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/handle.h>
#include <ananas/handle-options.h>
#include <ananas/lib.h>
#include <ananas/trace.h>
#include <ananas/thread.h>

TRACE_SETUP;

static errorcode_t
refhandle_get_handle(struct HANDLE* handle, thread_t** out_thread, struct HANDLE** out_handle, handleindex_t* out_index)
{
	KASSERT(handle->h_type == HANDLE_TYPE_REFERENCE, "bad handle type");
	*out_thread = handle->h_data.d_ref.ref_thread;
	*out_handle = handle->h_data.d_ref.ref_handle;
	*out_index = handle->h_data.d_ref.ref_index;
	return ANANAS_ERROR_OK;
}

static errorcode_t
refhandle_read(thread_t* thread, handleindex_t index, struct HANDLE* handle, void* buf, size_t* len)
{
	thread_t* ref_thread;
	struct HANDLE* ref_handle;
	handleindex_t ref_index;

	errorcode_t err = refhandle_get_handle(handle, &ref_thread, &ref_handle, &ref_index);
	ANANAS_ERROR_RETURN(err);
	if (ref_handle->h_hops->hop_read == NULL)
		return ANANAS_ERROR(BAD_OPERATION);
	return ref_handle->h_hops->hop_read(ref_thread, ref_index, ref_handle, buf, len);
}

static errorcode_t
refhandle_write(thread_t* thread, handleindex_t index, struct HANDLE* handle, const void* buf, size_t* len)
{
	thread_t* ref_thread;
	struct HANDLE* ref_handle;
	handleindex_t ref_index;

	errorcode_t err = refhandle_get_handle(handle, &ref_thread, &ref_handle, &ref_index);
	ANANAS_ERROR_RETURN(err);
	if (ref_handle->h_hops->hop_write == NULL)
		return ANANAS_ERROR(BAD_OPERATION);
	return ref_handle->h_hops->hop_write(ref_thread, ref_index, ref_handle, buf, len);
}

static errorcode_t
refhandle_free(thread_t* thread, struct HANDLE* handle)
{
	thread_t* ref_thread;
	struct HANDLE* ref_handle;
	handleindex_t ref_index;

	errorcode_t err = refhandle_get_handle(handle, &ref_thread, &ref_handle, &ref_index);
	ANANAS_ERROR_RETURN(err);

	return handle_free(ref_handle);
}

static errorcode_t
refhandle_unlink(thread_t* thread, handleindex_t index, struct HANDLE* handle)
{
	thread_t* ref_thread;
	struct HANDLE* ref_handle;
	handleindex_t ref_index;

	errorcode_t err = refhandle_get_handle(handle, &ref_thread, &ref_handle, &ref_index);
	ANANAS_ERROR_RETURN(err);

	if (ref_handle->h_hops->hop_unlink == NULL)
		return ANANAS_ERROR(BAD_OPERATION);
	return ref_handle->h_hops->hop_unlink(ref_thread, ref_index, ref_handle);
}

static errorcode_t
refhandle_control(thread_t* thread, handleindex_t index, struct HANDLE* handle, unsigned int op, void* arg, size_t len)
{
	thread_t* ref_thread;
	struct HANDLE* ref_handle;
	handleindex_t ref_index;

	errorcode_t err = refhandle_get_handle(handle, &ref_thread, &ref_handle, &ref_index);
	ANANAS_ERROR_RETURN(err);

	if (ref_handle->h_hops->hop_control == NULL)
		return ANANAS_ERROR(BAD_OPERATION);
	return ref_handle->h_hops->hop_control(ref_thread, ref_index, ref_handle, op, arg, len);
}

static errorcode_t
refhandle_clone(thread_t* thread_in, handleindex_t index, struct HANDLE* handle, struct CLONE_OPTIONS* opts, thread_t* thread_out, struct HANDLE** handle_out, handleindex_t* index_out)
{
	thread_t* ref_thread;
	struct HANDLE* ref_handle;
	handleindex_t ref_index;

	errorcode_t err = refhandle_get_handle(handle, &ref_thread, &ref_handle, &ref_index);
	ANANAS_ERROR_RETURN(err);
	return handle_create_ref(ref_thread, ref_index, thread_out, handle_out, index_out);
}

static errorcode_t
refhandle_summon(thread_t* thread, struct HANDLE* handle, struct SUMMON_OPTIONS* opts, struct HANDLE** handle_out, handleindex_t* index_out)
{
	thread_t* ref_thread;
	struct HANDLE* ref_handle;
	handleindex_t ref_index;

	errorcode_t err = refhandle_get_handle(handle, &ref_thread, &ref_handle, &ref_index);
	ANANAS_ERROR_RETURN(err);
	if (ref_handle->h_hops->hop_summon == NULL)
		return ANANAS_ERROR(BAD_OPERATION);
	return ref_handle->h_hops->hop_summon(thread, ref_handle, opts, handle_out, index_out);
}

static struct HANDLE_OPS ref_hops = {
	.hop_read = refhandle_read,
	.hop_write = refhandle_write,
	.hop_open = NULL, /* can't open a ref handle */
	.hop_free = refhandle_free,
	.hop_unlink = refhandle_unlink,
	.hop_create = NULL, /* can't directly create ref handles - use clone on the parent */
	.hop_control = refhandle_control,
	.hop_clone = refhandle_clone,
	.hop_summon = refhandle_summon
};

HANDLE_TYPE(HANDLE_TYPE_REFERENCE, "reference", ref_hops);

/* vim:set ts=2 sw=2: */
