#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/handle.h"
#include "kernel/trace.h"
#include "kernel/result.h"
#include "kernel/vm.h"
#include "syscall.h"

TRACE_SETUP;

Result
sys_read(Thread* t, handleindex_t hindex, void* buf, size_t* len)
{
	TRACE(SYSCALL, FUNC, "t=%p, hindex=%u, buf=%p, len=%p", t, hindex, buf, len);

	/* Get the handle */
	struct HANDLE* h;
	RESULT_PROPAGATE_FAILURE(
		syscall_get_handle(*t, hindex, &h)
	);

	/* Fetch the size operand */
	size_t size;
	RESULT_PROPAGATE_FAILURE(
		syscall_fetch_size(*t, len, &size)
	);

	/* Attempt to map the buffer write-only */
	void* buffer;
	RESULT_PROPAGATE_FAILURE(
		syscall_map_buffer(*t, buf, size, VM_FLAG_WRITE, &buffer)
	);

	/* And read data to it */
	Result result = RESULT_MAKE_FAILURE(EINVAL);
	if (h->h_hops->hop_read != NULL) {
		RESULT_PROPAGATE_FAILURE(
			h->h_hops->hop_read(t, hindex, h, buf, &size)
		);
	} else {
		return RESULT_MAKE_FAILURE(EINVAL);
	}

	/* Finally, inform the user of the length read - the read went OK */
	RESULT_PROPAGATE_FAILURE(
		syscall_set_size(*t, len, size)
	);

	TRACE(SYSCALL, FUNC, "t=%p, success: size=%u", t, size);
	return Result::Success();
}

