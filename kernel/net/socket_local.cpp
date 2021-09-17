#include <ananas/errno.h>
#include "kernel/net/socket_local.h"
#include "kernel/fd.h"
#include "kernel/init.h"
#include "kernel/process.h"

namespace net
{
    namespace
    {
        Result local_read(fdindex_t index, FD& fd, void* buf, size_t len)
        {
            return Result::Failure(EINVAL);
        }

        Result local_write(fdindex_t index, FD& fd, const void* buf, size_t len)
        {
            return Result::Failure(EINVAL);
        }

        Result local_open(fdindex_t index, FD& fd, const char* path, int flags, int mode)
        {
            return Result::Failure(EINVAL);
        }

        Result local_free(Process& proc, FD& fd)
        {
            return Result::Failure(EINVAL);
        }
        Result local_unlink(fdindex_t index, FD& fd)
        {
            return Result::Failure(EINVAL);
        }

        Result local_clone(Process& proc_in, fdindex_t index, FD& fd_in, struct CLONE_OPTIONS* opts, Process& proc_out, FD*& fd_out, fdindex_t index_out_min, fdindex_t& index_out)
        {
            return Result::Failure(EINVAL);
        }

        Result local_ioctl(fdindex_t index, FD& fd, unsigned long request, void* args[])
        {
            return Result::Failure(EINVAL);
        }

        Result local_accept(fdindex_t, FD& fd, struct sockaddr*, socklen_t*)
        {
            return Result::Failure(EINVAL);
        }

        Result local_bind(fdindex_t, FD& fd, const struct sockaddr*, socklen_t)
        {
            return Result::Failure(EINVAL);
        }

        Result local_connect(fdindex_t, FD& fd, const struct sockaddr*, socklen_t)
        {
            return Result::Failure(EINVAL);
        }

        Result local_listen(fdindex_t, FD& fd, int)
        {
            return Result::Failure(EINVAL);
        }

        Result local_send(fdindex_t, FD& fd, const void*, size_t, int)
        {
            return Result::Failure(EINVAL);
        }

        Result local_select(fdindex_t, FD& fd, fd_set*, fd_set*, fd_set*, struct timeval*)
        {
            return Result::Failure(EINVAL);
        }

        bool local_can_read(fdindex_t, FD&)
        {
            return false;
        }

        bool local_can_write(fdindex_t, FD&)
        {
            return false;
        }

        bool local_has_except(fdindex_t, FD&)
        {
            return false;
        }

        FDOperations local_fdops = {
            .d_read = local_read,
            .d_write = local_write,
            .d_open = local_open,
            .d_free = local_free,
            .d_unlink = local_unlink,
            .d_clone = local_clone,
            .d_ioctl = local_ioctl,
            .d_accept = local_accept,
            .d_bind = local_bind,
            .d_connect = local_connect,
            .d_listen = local_listen,
            .d_send = local_send,
            .d_can_read = local_can_read,
            .d_can_write = local_can_write,
            .d_has_except = local_has_except
        };

        const init::OnInit registerFDType(init::SubSystem::Handle, init::Order::Second, []() {
            static FDType ft("local", FD_TYPE_SOCKET, local_fdops);
            fd::RegisterType(ft);
        });
    }

    Result AllocateLocalSocket()
    {
        auto& proc = process::GetCurrent();

        /* Obtain a new handle */
        FD* fd;
        fdindex_t index_out;
        if (auto result = fd::Allocate(FD_TYPE_SOCKET, proc, 0, fd, index_out); result.IsFailure())
            return result;

        return Result::Success(index_out);
    }
}
