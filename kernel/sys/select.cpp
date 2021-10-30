/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include <ananas/errno.h>
#include "kernel/lib.h"
#include <ananas/util/vector.h>
#include <sys/select.h>
#include "kernel/fd.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/time.h"
#include "syscall.h"

#include "kernel/process.h"

namespace
{
    constexpr inline auto debugSelect = false;

    struct SelectFd {
        fdindex_t index;
        FD* fd;
    };
    using SelectVector = util::vector<SelectFd>;

    enum class FdSetType { Read, Write, Except };

    // XXX This would make more sense once we can lock each FD
    Result ConvertFdSetToSelectVector(fd_set* fds, FdSetType type, SelectVector& vec)
    {
        if (fds == nullptr) return Result::Success();

        constexpr auto fdsLength = sizeof(fd_set::fds_bits) / sizeof(fd_set::fds_bits[0]);
        for(int n = 0; n < fdsLength * FD_BITS_PER_FDS; ++n) {
            if (!FD_ISSET(n, fds)) continue;
            FD* fd;
            if (auto result = syscall_get_fd(FD_TYPE_ANY, n, fd); result.IsFailure())
                return result;

            switch(type) {
                case FdSetType::Read:
                    if (fd->fd_ops->d_can_read == nullptr) return Result::Failure(EINVAL);
                    break;
                case FdSetType::Write:
                    if (fd->fd_ops->d_can_write == nullptr) return Result::Failure(EINVAL);
                    break;
                case FdSetType::Except:
                    if (fd->fd_ops->d_has_except == nullptr) return Result::Failure(EINVAL);
                    break;
            }
            vec.push_back({ n, fd } );
        }
        return Result::Success();
    }

    template<typename Pred>
    int ProcessSelectVector(SelectVector& vec, fd_set* fds, Pred pred)
    {
        int num_events = 0;
        for(const auto& selectfd: vec) {
            const auto index = selectfd.index;
            auto& fd = *selectfd.fd;
            if (pred(index, fd)) {
                FD_SET(index, fds);
                ++num_events;
            }
        }
        return num_events;
    }
}

Result sys_select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* errorfds, struct timeval* timeout)
{
    SelectVector read_fds, write_fds, error_fds;
    if (auto result = ConvertFdSetToSelectVector(readfds, FdSetType::Read, read_fds); result.IsFailure())
        return result;
    if (auto result = ConvertFdSetToSelectVector(writefds, FdSetType::Write, write_fds); result.IsFailure())
        return result;
    if (auto result = ConvertFdSetToSelectVector(errorfds, FdSetType::Except, error_fds); result.IsFailure())
        return result;

    if (debugSelect) kprintf("sys_select pid %d -> %d %d %d\n", process::GetCurrent().p_pid, read_fds.size(), write_fds.size(), error_fds.size());

    if (readfds != nullptr) FD_ZERO(readfds);
    if (writefds != nullptr) FD_ZERO(writefds);
    if (errorfds != nullptr) FD_ZERO(errorfds);

    const auto lastTick = timeout != nullptr ? time::GetTicks() + time::TimevalToTicks(*timeout) : 0;
    while(true) {
        int num_events = 0;
        num_events += ProcessSelectVector(read_fds, readfds, [](int index, FD& fd) {
            return fd.fd_ops->d_can_read(index, fd);
        });
        num_events += ProcessSelectVector(write_fds, writefds, [](int index, FD& fd) {
            return fd.fd_ops->d_can_write(index, fd);
        });
        num_events += ProcessSelectVector(error_fds, errorfds, [](int index, FD& fd) {
            return fd.fd_ops->d_has_except(index, fd);
        });
        if (num_events) {
            if (debugSelect) kprintf("select: pid %d; %d event(s)\n", process::GetCurrent().p_pid, num_events);
            return Result::Success(num_events);
        }

        if (timeout != nullptr && time::IsTickAfter(time::GetTicks(), lastTick)) {
            if (debugSelect) kprintf("select: pid %d; TIMEOUT, %d event(s)\n", process::GetCurrent().p_pid, num_events);
            return Result::Success(num_events);
        }

        // No events yet - wait XXX event-driven
        thread_sleep_ms(1);
    }
    // NOTREACHED
}
