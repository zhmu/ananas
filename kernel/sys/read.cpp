#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscall.h>
#include <ananas/trace.h>
#include <ananas/vm.h>

TRACE_SETUP;

errorcode_t
sys_read(thread_t* t, handleindex_t hindex, void* buf, size_t* len)
{
	TRACE(SYSCALL, FUNC, "t=%p, hindex=%u, buf=%p, len=%p", t, hindex, buf, len);
	errorcode_t err;

	/* Get the handle */
	struct HANDLE* h;
	err = syscall_get_handle(t, hindex, &h);
	ANANAS_ERROR_RETURN(err);

	/* Fetch the size operand */
	size_t size;
	err = syscall_fetch_size(t, len, &size);
	ANANAS_ERROR_RETURN(err);

	/* Attempt to map the buffer write-only */
	void* buffer;
	err = syscall_map_buffer(t, buf, size, VM_FLAG_WRITE, &buffer);
	ANANAS_ERROR_RETURN(err);

	/* And read data to it */
	if (h->h_hops->hop_read != NULL)
		err = h->h_hops->hop_read(t, hindex, h, buf, &size);
	else
		err = ANANAS_ERROR(BAD_OPERATION);

	/* Finally, inform the user of the length read - the read went OK */
	err = syscall_set_size(t, len, size);
	ANANAS_ERROR_RETURN(err);

	TRACE(SYSCALL, FUNC, "t=%p, success: size=%u", t, size);
	return err;
}

