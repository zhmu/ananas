#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/trace.h"
#include "kernel/vm.h"
#include "syscall.h"

TRACE_SETUP;

errorcode_t
sys_write(thread_t* t, handleindex_t hindex, const void* buf, size_t* len)
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

	/* Attempt to map the buffer readonly */
	void* buffer;
	err = syscall_map_buffer(t, buf, size, VM_FLAG_READ, &buffer);
	ANANAS_ERROR_RETURN(err);

	/* And write data from to it */
	if (h->h_hops->hop_write != NULL)
		err = h->h_hops->hop_write(t, hindex, h, buf, &size);
	else
		err = ANANAS_ERROR(BAD_OPERATION);
	ANANAS_ERROR_RETURN(err);

	/* Finally, inform the user of the length read - the read went OK */
	err = syscall_set_size(t, len, size);
	ANANAS_ERROR_RETURN(err);

	TRACE(SYSCALL, FUNC, "t=%p, success: size=%u", t, size);
	return err;
}
