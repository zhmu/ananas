#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/flags.h>
#include "kernel/fd.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "syscall.h"

TRACE_SETUP;

Result sys_fcntl(Thread* t, fdindex_t hindex, int cmd, const void* in, void* out)
{
    TRACE(SYSCALL, FUNC, "t=%p, hindex=%d cmd=%d", t, hindex, cmd);
    Process& process = *t->t_process;

    /* Get the handle */
    FD* fd;
    if (auto result = syscall_get_fd(*t, FD_TYPE_ANY, hindex, fd); result.IsFailure())
        return result;

    switch (cmd) {
        case F_DUPFD: {
            int min_fd = (int)(uintptr_t)in;
            FD* fd_out;
            fdindex_t idx_out = -1;
            if (auto result = fd::Clone(process, hindex, nullptr, process, fd_out, min_fd, idx_out);
                result.IsFailure())
                return result;
            return Result::Success(idx_out);
        }
        case F_GETFD:
            /* TODO */
            return Result::Success(0);
        case F_GETFL:
            /* TODO */
            return Result::Success(0);
        case F_SETFD: {
            return RESULT_MAKE_FAILURE(EPERM);
        }
        case F_SETFL: {
            return RESULT_MAKE_FAILURE(EPERM);
        }
        default:
            return RESULT_MAKE_FAILURE(EINVAL);
    }

    // NOTREACHED
}
