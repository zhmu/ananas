#include <ananas/types.h>
#include "kernel/handle.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/vm.h"
#include "syscall.h"

TRACE_SETUP;

Result
sys_close(Thread* t, handleindex_t hindex)
{
	TRACE(SYSCALL, FUNC, "t=%p, hindex=%p", t, hindex);

	struct HANDLE* h;
	RESULT_PROPAGATE_FAILURE(
		syscall_get_handle(*t, hindex, &h)
	);
	RESULT_PROPAGATE_FAILURE(
		handle_free(h)
	);

	TRACE(SYSCALL, INFO, "t=%p, success", t);
	return Result::Success();
}

