#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/handle.h>
#include <ananas/handle-options.h>
#include <ananas/trace.h>
#include <ananas/thread.h>
#include <ananas/mm.h>
#include <ananas/lib.h>

TRACE_SETUP;

static errorcode_t
pipehandle_create(thread_t* thread, struct HANDLE* handle, struct CREATE_OPTIONS* opts)
{
	if (opts->cr_flags != CREATE_PIPE_FLAG_MASK )
		return ANANAS_ERROR(BAD_FLAG);
	if (opts->cr_length <= 0 || opts->cr_length > CREATE_PIPE_MAX_LENGTH)
		return ANANAS_ERROR(BAD_LENGTH);

	/* Set the pipe up */
	struct HANDLE_PIPE_INFO* hpi = &handle->h_data.d_pipe;
	hpi->hpi_buffer = kmalloc(opts->cr_length); /* XXX should check error */
	memset(hpi->hpi_buffer, 0, opts->cr_length); /* prevent information leakage */
	CBUFFER_INIT(hpi, hpi->hpi_buffer, opts->cr_length);
	return ANANAS_ERROR_OK;
}

static errorcode_t
pipehandle_free(thread_t* thread, struct HANDLE* handle)
{
	kfree(handle->h_data.d_pipe.hpi_buffer);
	return ANANAS_ERROR_OK;
}

static errorcode_t
pipehandle_read(thread_t* thread, struct HANDLE* handle, void* buf, size_t* len)
{
	struct HANDLE_PIPE_INFO* hpi = &handle->h_data.d_pipe;
	size_t n = CBUFFER_READ(hpi, buf, *len);
	if (n > 0) {
		/* Something was read; we should signal the read */
		handle_signal(handle, HANDLE_EVENT_READ, n);
	}
	*len = n;
	return ANANAS_ERROR_OK;
}

static errorcode_t
pipehandle_write(thread_t* thread, struct HANDLE* handle, const void* buf, size_t* len)
{
	struct HANDLE_PIPE_INFO* hpi = &handle->h_data.d_pipe;
	size_t n = CBUFFER_WRITE(hpi, buf, *len);
	if (n > 0) {
		/* Something was written; we should signal the write */
		handle_signal(handle, HANDLE_EVENT_WRITE, n);
	}
	*len = n;
	return ANANAS_ERROR_OK;
}

static errorcode_t
pipehandle_clone(thread_t* thread, struct HANDLE* handle, struct CLONE_OPTIONS* opts, struct HANDLE** result)
{
	/* We just create a new reference to the pipe handle */
	return handle_create_ref_locked(thread, handle, result);
}

static struct HANDLE_OPS pipe_hops = {
	.hop_create = pipehandle_create,
	.hop_free = pipehandle_free,
	.hop_read = pipehandle_read,
	.hop_write = pipehandle_write,
	.hop_clone = pipehandle_clone,
};
HANDLE_TYPE(HANDLE_TYPE_PIPE, "pipe", pipe_hops);

/* vim:set ts=2 sw=2: */
