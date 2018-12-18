#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/flags.h>
#include "kernel/fd.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"

TRACE_SETUP;

Result sys_open(Thread* t, const char* path, int flags, int mode)
{
    TRACE(SYSCALL, FUNC, "t=%p, path='%s', flags=%d, mode=%o", t, path, flags, mode);
    Process& proc = *t->t_process;

    /* Obtain a new handle */
    FD* fd;
    fdindex_t index_out;
    if (auto result = fd::Allocate(FD_TYPE_FILE, proc, 0, fd, index_out); result.IsFailure())
        return result;

    /*
     * Ask the handle to open the resource - if there isn't an open operation, we
     * assume this handle type cannot be opened using a syscall.
     */
    Result result = RESULT_MAKE_FAILURE(EINVAL);
    if (fd->fd_ops->d_open != NULL)
        result = fd->fd_ops->d_open(t, index_out, *fd, path, flags, mode);

    if (result.IsFailure()) {
        TRACE(SYSCALL, FUNC, "t=%p, error %u", t, result.AsStatusCode());
        fd->Close();
        return result;
    }
    TRACE(SYSCALL, FUNC, "t=%p, success, hindex=%u", t, index_out);
    return Result::Success(index_out);
}
