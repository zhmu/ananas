#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/handle.h>
#include <ananas/handle-options.h>
#include <ananas/trace.h>
#include <ananas/thread.h>
#include <ananas/lib.h>

TRACE_SETUP;

static errorcode_t
memoryhandle_create(thread_t* thread, struct HANDLE* handle, struct CREATE_OPTIONS* opts)
{
	/* See if the flags mask works */
	if ((opts->cr_flags & ~(CREATE_MEMORY_FLAG_MASK)) != 0)
		return ANANAS_ERROR(BAD_FLAG);

	/* Fetch memory as needed */
	int map_flags = THREAD_MAP_ALLOC;
	if (opts->cr_flags & CREATE_MEMORY_FLAG_READ)    map_flags |= THREAD_MAP_READ;
	if (opts->cr_flags & CREATE_MEMORY_FLAG_WRITE)   map_flags |= THREAD_MAP_WRITE;
	if (opts->cr_flags & CREATE_MEMORY_FLAG_EXECUTE) map_flags |= THREAD_MAP_EXECUTE;
	struct THREAD_MAPPING* tm;
	errorcode_t err = thread_map(thread, (addr_t)NULL, opts->cr_length, map_flags, &tm);
	ANANAS_ERROR_RETURN(err);

	handle->data.memory.mapping = tm;
	handle->data.memory.addr = (void*)tm->tm_virt;
	handle->data.memory.length = tm->tm_len;
	return ANANAS_ERROR_OK;
}

static errorcode_t
memoryhandle_free(thread_t* thread, struct HANDLE* handle)
{
	thread_free_mapping(handle->thread, handle->data.memory.mapping);
	return ANANAS_ERROR_OK;
}

static errorcode_t
memoryhandle_control(thread_t* thread, struct HANDLE* handle, unsigned int op, void* arg, size_t len)
{
	switch(op) {
		case HCTL_MEMORY_GET_INFO: {
			/* Ensure the structure length is sane */
			struct HCTL_MEMORY_GET_INFO_ARG* in = arg;
			if (arg == NULL)
				return ANANAS_ERROR(BAD_ADDRESS);
			if (len != sizeof(*in))
				return ANANAS_ERROR(BAD_LENGTH);
			in->in_base = handle->data.memory.addr;
			in->in_length = handle->data.memory.length;
			return ANANAS_ERROR_OK;
		}
	}

	/* What's this? */
	return ANANAS_ERROR(BAD_SYSCALL);
}

static struct HANDLE_OPS memory_hops = {
	.hop_create = memoryhandle_create,
	.hop_free = memoryhandle_free,
	.hop_control = memoryhandle_control
};
HANDLE_TYPE(HANDLE_TYPE_MEMORY, "memory", memory_hops);

/* vim:set ts=2 sw=2: */
