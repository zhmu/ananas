#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/handle.h>
#include <ananas/handle-options.h>
#include <ananas/trace.h>
#include <ananas/thread.h>
#include <ananas/vm.h>
#include <ananas/vmspace.h>
#include <ananas/lib.h>

TRACE_SETUP;

static errorcode_t
memoryhandle_create(thread_t* thread, handleindex_t index, struct HANDLE* handle, struct CREATE_OPTIONS* opts)
{
	/* See if the flags mask works */
	if ((opts->cr_flags & ~(CREATE_MEMORY_FLAG_MASK)) != 0)
		return ANANAS_ERROR(BAD_FLAG);

	/* Fetch memory as needed */
	int map_flags = VM_FLAG_ALLOC | VM_FLAG_USER;
	if (opts->cr_flags & CREATE_MEMORY_FLAG_READ)    map_flags |= VM_FLAG_READ;
	if (opts->cr_flags & CREATE_MEMORY_FLAG_WRITE)   map_flags |= VM_FLAG_WRITE;
	if (opts->cr_flags & CREATE_MEMORY_FLAG_EXECUTE) map_flags |= VM_FLAG_EXECUTE;
	struct VM_AREA* va;
	errorcode_t err = vmspace_map(thread->t_vmspace, (addr_t)NULL, opts->cr_length, map_flags, &va);
	ANANAS_ERROR_RETURN(err);

	handle->h_data.d_memory.hmi_vmarea = va;
	return ANANAS_ERROR_OK;
}

static errorcode_t
memoryhandle_free(thread_t* thread, struct HANDLE* handle)
{
	vmspace_area_free(thread->t_vmspace, handle->h_data.d_memory.hmi_vmarea);
	return ANANAS_ERROR_OK;
}

static errorcode_t
memoryhandle_control(thread_t* thread, handleindex_t index, struct HANDLE* handle, unsigned int op, void* arg, size_t len)
{
	struct VM_AREA* va = handle->h_data.d_memory.hmi_vmarea;

	switch(op) {
		case HCTL_MEMORY_GET_INFO: {
			/* Ensure the structure length is sane */
			struct HCTL_MEMORY_GET_INFO_ARG* in = arg;
			if (arg == NULL)
				return ANANAS_ERROR(BAD_ADDRESS);
			if (len != sizeof(*in))
				return ANANAS_ERROR(BAD_LENGTH);
			in->in_base = (void*)va->va_virt;
			in->in_length = va->va_len;
			return ANANAS_ERROR_OK;
		}
		case HCTL_MEMORY_RESIZE: {
			/* arg is to be NULL, len is the new size in pages */
			if (arg != NULL)
				return ANANAS_ERROR(BAD_ADDRESS);
			return vmspace_area_resize(thread->t_vmspace, va, len);
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
