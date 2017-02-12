#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscall.h>
#include <ananas/trace.h>
#include <ananas/vm.h>

TRACE_SETUP;

errorcode_t
sys_close(thread_t* t, handleindex_t hindex)
{
	TRACE(SYSCALL, FUNC, "t=%p, hindex=%p", t, hindex);

	struct HANDLE* h;
	errorcode_t err = syscall_get_handle(t, hindex, &h);
	ANANAS_ERROR_RETURN(err);
	err = handle_free(h);
	ANANAS_ERROR_RETURN(err);

	TRACE(SYSCALL, INFO, "t=%p, success", t);
	return err;
}

