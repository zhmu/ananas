#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/handle.h>
#include <ananas/handle-options.h>
#include <ananas/lib.h>
#include <ananas/trace.h>
#include <ananas/thread.h>

TRACE_SETUP;

static errorcode_t
refhandle_get_handle(struct HANDLE* handle, struct HANDLE** out)
{
	KASSERT(handle->h_type == HANDLE_TYPE_REFERENCE, "bad handle type");
	*out = handle->h_data.d_handle;
	return ANANAS_ERROR_OK;
}

static errorcode_t
refhandle_read(thread_t* thread, struct HANDLE* handle, void* buf, size_t* len)
{
	struct HANDLE* h;
	errorcode_t err = refhandle_get_handle(handle, &h);
	ANANAS_ERROR_RETURN(err);
	if (h->h_hops->hop_read == NULL)
		return ANANAS_ERROR(BAD_OPERATION);
	return h->h_hops->hop_read(thread, h, buf, len);
}

static errorcode_t
refhandle_write(thread_t* thread, struct HANDLE* handle, const void* buf, size_t* len)
{
	struct HANDLE* h;
	errorcode_t err = refhandle_get_handle(handle, &h);
	ANANAS_ERROR_RETURN(err);
	if (h->h_hops->hop_write == NULL)
		return ANANAS_ERROR(BAD_OPERATION);
	return h->h_hops->hop_write(thread, h, buf, len);
}

static errorcode_t
refhandle_free(thread_t* thread, struct HANDLE* handle)
{
	struct HANDLE* h;
	errorcode_t err = refhandle_get_handle(handle, &h);
	ANANAS_ERROR_RETURN(err);
	if (h->h_hops->hop_free == NULL)
		return ANANAS_ERROR(BAD_OPERATION);
	return h->h_hops->hop_free(thread, h);
}

static errorcode_t
refhandle_unlink(thread_t* thread, struct HANDLE* handle)
{
	struct HANDLE* h;
	errorcode_t err = refhandle_get_handle(handle, &h);
	ANANAS_ERROR_RETURN(err);
	if (h->h_hops->hop_unlink == NULL)
		return ANANAS_ERROR(BAD_OPERATION);
	return h->h_hops->hop_unlink(thread, h);
}

static errorcode_t
refhandle_control(thread_t* thread, struct HANDLE* handle, unsigned int op, void* arg, size_t len)
{
	struct HANDLE* h;
	errorcode_t err = refhandle_get_handle(handle, &h);
	ANANAS_ERROR_RETURN(err);
	if (h->h_hops->hop_control == NULL)
		return ANANAS_ERROR(BAD_OPERATION);
	return h->h_hops->hop_control(thread, h, op, arg, len);
}

static errorcode_t
refhandle_clone(thread_t* thread, struct HANDLE* handle, struct HANDLE** out)
{
	struct HANDLE* h;
	errorcode_t err = refhandle_get_handle(handle, &h);
	ANANAS_ERROR_RETURN(err);
	return handle_create_ref(thread, h, out);
}

static errorcode_t
refhandle_summon(thread_t* thread, struct HANDLE* handle, struct SUMMON_OPTIONS* opts, struct HANDLE** out)
{
	struct HANDLE* h;
	errorcode_t err = refhandle_get_handle(handle, &h);
	ANANAS_ERROR_RETURN(err);
	if (h->h_hops->hop_summon == NULL)
		return ANANAS_ERROR(BAD_OPERATION);
	return h->h_hops->hop_summon(thread, h, opts, out);
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

struct HANDLE_TYPE ref_handle_type = {
	.ht_name = "reference",
	.ht_id = HANDLE_TYPE_REFERENCE,
	.ht_hops = &ref_hops
};

/* vim:set ts=2 sw=2: */
