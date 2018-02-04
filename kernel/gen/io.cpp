#include <ananas/types.h>
#include <ananas/elf.h>
#include <ananas/handle-options.h>
#include <ananas/stat.h>
#include <ananas/syscalls.h>
#include "kernel/bio.h"
#include "kernel/device.h"
#include "kernel/handle.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/syscall.h"
#include "kernel/trace.h"
#include "kernel/vm.h"

TRACE_SETUP;

static Result
sys_handlectl_generic(thread_t* t, handleindex_t hindex, struct HANDLE* h, unsigned int op, void* arg, size_t len)
{
	Result result;

	switch(op) {
		case HCTL_GENERIC_STATUS: {
			/* Ensure we understand the arguments */
			struct HCTL_STATUS_ARG* sa = arg;
			if (arg == nullptr)
				return RESULT_MAKE_FAILURE(EINVAL);
			if (len != sizeof(*sa))
				return RESULT_MAKE_FAILURE(EINVAL);
			sa->sa_flags = 0;
			result = Result::Success();
			break;
		}
		default:
			/* What's this? */
			result = RESULT_MAKE_FAILURE(EINVAL);
	}
	return result;
}

Result
sys_handlectl(thread_t* t, handleindex_t hindex, unsigned int op, void* arg, size_t len)
{
	TRACE(SYSCALL, FUNC, "t=%p, hindex=%u, op=%u, arg=%p, len=0x%x", t, hindex, op, arg, len);
	Result err;

	/* Fetch the handle */
	struct HANDLE* h;
	RESULT_PROPAGATE_FAILURE(
		syscall_get_handle(t, hindex, &h)
	);

	/* Lock the handle; we don't want it leaving our sight */
	MutexLockGuard(&h->h_mutex);

	/* Obtain arguments (note that some calls do not have an argument) */
	void* handlectl_arg = nullptr;
	if (arg != nullptr) {
		RESULT_PROPAGATE_FAILURE(
			syscall_map_buffer(t, arg, len, VM_FLAG_READ | VM_FLAG_WRITE, &handlectl_arg)
		);
	}

	/* Handle generics here; handles aren't allowed to override them */
	if (op >= _HCTL_GENERIC_FIRST && op <= _HCTL_GENERIC_LAST) {
		return sys_handlectl_generic(t, hindex, h, op, handlectl_arg, len);

	/* Let the handle deal with this request */
	if (h->h_hops->hop_control != nullptr)
		return h->h_hops->hop_control(t, hindex, h, op, arg, len)

	return RESULT_MAKE_FAILURE(EINVAL);
}

/* vim:set ts=2 sw=2: */
