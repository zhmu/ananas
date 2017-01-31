#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/handle.h>
#include <ananas/handle-options.h>
#include <ananas/trace.h>
#include <ananas/thread.h>
#include <ananas/mm.h>
#include <ananas/lib.h>

#if 0
TRACE_SETUP;

/* Note that pipehandle_signal() signals everything *except* the hpi passed to it! */
static void
pipehandle_signal(struct HANDLE_PIPE_INFO* hpi, handle_event_t event, handle_event_result_t result)
{
	struct HANDLE_PIPE_INFO* cur_hpi = hpi->hpi_next;
	for (/* nothing */; cur_hpi != hpi; cur_hpi = cur_hpi->hpi_next) {
		handle_signal(cur_hpi->hpi_handle, event, result);
	}
}

static errorcode_t
pipehandle_create(thread_t* thread, handleindex_t index, struct HANDLE* handle, struct CREATE_OPTIONS* opts)
{
	if (opts->cr_flags != CREATE_PIPE_FLAG_MASK )
		return ANANAS_ERROR(BAD_FLAG);
	if (opts->cr_length <= 0 || opts->cr_length > CREATE_PIPE_MAX_LENGTH)
		return ANANAS_ERROR(BAD_LENGTH);

	/* Create a new pipe */
	struct HANDLE_PIPE_BUFFER* hpb = kmalloc(sizeof(struct HANDLE_PIPE_BUFFER) + opts->cr_length);
	/* XXX check error on hpb == NULL */
	mutex_init(&hpb->hpb_mutex, "pipe");
	hpb->hpb_read_count = 1;
	hpb->hpb_write_count = 1;
	memset(hpb->hpb_buffer, 0, opts->cr_length); /* prevent information leakage */
	CBUFFER_INIT(hpb, hpb->hpb_buffer, opts->cr_length);

	/* Hook the pipe buffer to the pipe handle */
	struct HANDLE_PIPE_INFO* hpi = &handle->h_data.d_pipe;
	hpi->hpi_flags = HPI_FLAG_READ | HPI_FLAG_WRITE;
	hpi->hpi_buffer = hpb;
	hpi->hpi_next = hpi;
	hpi->hpi_handle = handle;
	return ANANAS_ERROR_NONE;
}

static errorcode_t
pipehandle_free(thread_t* thread, struct HANDLE* handle)
{
	struct HANDLE_PIPE_INFO* hpi = &handle->h_data.d_pipe;
	struct HANDLE_PIPE_BUFFER* hpb = hpi->hpi_buffer;
	mutex_lock(&hpb->hpb_mutex);
	if (hpi->hpi_flags & HPI_FLAG_READ)
		hpb->hpb_read_count--;
	KASSERT(hpb->hpb_read_count >= 0, "read_count < 0");
	if (hpi->hpi_flags & HPI_FLAG_WRITE)
		hpb->hpb_write_count--;
	KASSERT(hpb->hpb_write_count >= 0, "write_count < 0");
	if (hpb->hpb_read_count != 0 || hpb->hpb_write_count != 0) {
		/*
		 * Still readers or writers available; don't free the pipe. If we just
		 * freed a final reader of writer, signal accordingly.
		 */
		if (hpb->hpb_read_count == 0) {
			pipehandle_signal(hpi, HANDLE_EVENT_READ, 0);
		} else if (hpb->hpb_write_count == 0) {
			pipehandle_signal(hpi, HANDLE_EVENT_WRITE, 0);
		}

		/*
	 	 * Remove ourselves from the linked list; this works by finding our parent
		 * (i.e. the one who has our hpi in their hpi_next field) and replacing it
		 * with our hpi_next.
		 */
		struct HANDLE_PIPE_INFO* parent_hpi = hpi->hpi_next;
		while (parent_hpi->hpi_next != hpi)
			parent_hpi = parent_hpi->hpi_next;
		parent_hpi->hpi_next = hpi->hpi_next;

		mutex_unlock(&hpb->hpb_mutex);
		return ANANAS_ERROR_NONE;
	}
	
	/* OK, we can free the buffer */
	mutex_unlock(&hpb->hpb_mutex);
	hpi->hpi_buffer = NULL;
	kfree(hpb);
	return ANANAS_ERROR_NONE;
}

static errorcode_t
pipehandle_read(thread_t* thread, handleindex_t index, struct HANDLE* handle, void* buf, size_t* len)
{
	struct HANDLE_PIPE_INFO* hpi = &handle->h_data.d_pipe;
	if ((hpi->hpi_flags & HPI_FLAG_READ) == 0)
		return ANANAS_ERROR(BAD_OPERATION);

	struct HANDLE_PIPE_BUFFER* hpb = hpi->hpi_buffer;
	mutex_lock(&hpb->hpb_mutex);
	size_t n = CBUFFER_READ(hpb, buf, *len);
	if (n > 0) {
		/* Something was read; we should signal the read */
		pipehandle_signal(hpi, HANDLE_EVENT_READ, n);
	}
	mutex_unlock(&hpb->hpb_mutex);
	*len = n;
	return ANANAS_ERROR_NONE;
}

static errorcode_t
pipehandle_write(thread_t* thread, handleindex_t index, struct HANDLE* handle, const void* buf, size_t* len)
{
	struct HANDLE_PIPE_INFO* hpi = &handle->h_data.d_pipe;
	if ((hpi->hpi_flags & HPI_FLAG_WRITE) == 0)
		return ANANAS_ERROR(BAD_OPERATION);

	struct HANDLE_PIPE_BUFFER* hpb = hpi->hpi_buffer;
	mutex_lock(&hpb->hpb_mutex);
	size_t n = CBUFFER_WRITE(hpb, buf, *len);
	if (n > 0) {
		/* Something was written; we should signal the write */
		pipehandle_signal(hpi, HANDLE_EVENT_WRITE, n);
	}
	mutex_unlock(&hpb->hpb_mutex);
	*len = n;
	return ANANAS_ERROR_NONE;
}

static errorcode_t
pipehandle_clone(thread_t* thread_in, handleindex_t index, struct HANDLE* handle, struct CLONE_OPTIONS* opts, thread_t* thread_out, struct HANDLE** handle_out, handleindex_t* index_out)
{
	/*
 	 * Inspect the flags; if the verdict is an unusable handle, refuse to create
	 * it.
	 *
	 * XXX Should we disallow a readonly handle to be cloned writeonly?
	 */
	struct HANDLE_PIPE_INFO* parent_hpi = &handle->h_data.d_pipe;
	int flags = 0;
	if (opts != NULL) {
		if (opts->cl_flags & CLONE_FLAG_READONLY)
			flags |= HPI_FLAG_READ;
		if (opts->cl_flags & CLONE_FLAG_WRITEONLY)
			flags |= HPI_FLAG_WRITE;
	} else {
		/* Default to read/write clone if no flags are present */
		flags = HPI_FLAG_READ | HPI_FLAG_WRITE;
	}
	if (flags == 0) {
		/* No flags given; recycle what the original handle had */
		flags = parent_hpi->hpi_flags;
	}

	/* Allocate a brand new handle for the thread */
	errorcode_t err = handle_alloc(HANDLE_TYPE_PIPE, thread_out, 0, handle_out, index_out);
	ANANAS_ERROR_RETURN(err);

	/* Lock the pipe; we don't want anyone throwing it away while we hook it */
	struct HANDLE_PIPE_BUFFER* hpb = parent_hpi->hpi_buffer;
	mutex_lock(&hpb->hpb_mutex);

	/* Hook the pipe to the new handle */
	struct HANDLE_PIPE_INFO* hpi = &(*handle_out)->h_data.d_pipe;
	hpi->hpi_flags = flags;
	hpi->hpi_buffer = hpb;
	hpi->hpi_handle = *handle_out;
	if (flags & HPI_FLAG_READ)
		hpb->hpb_read_count++;
	if (flags & HPI_FLAG_WRITE)
		hpb->hpb_write_count++;

	/* Hook our handle to the handle chain */
	hpi->hpi_next = parent_hpi->hpi_next;
	parent_hpi->hpi_next = hpi;

	/* All done */
	mutex_unlock(&hpb->hpb_mutex);
	return err;
}

static struct HANDLE_OPS pipe_hops = {
	.hop_create = pipehandle_create,
	.hop_free = pipehandle_free,
	.hop_read = pipehandle_read,
	.hop_write = pipehandle_write,
	.hop_clone = pipehandle_clone,
};
HANDLE_TYPE(HANDLE_TYPE_PIPE, "pipe", pipe_hops);
#endif

/* vim:set ts=2 sw=2: */
