/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include <ananas/errno.h>
#include <sys/shm.h>
#include "kernel/shm.h"
#include "kernel/result.h"
#include "syscall.h"

Result sys_shmat(int shmid, const void* shmaddr, int shmflg, void** out)
{
    // XXX for now
    if (shmaddr != nullptr) return Result::Failure(EINVAL);
    if (shmflg != 0) return Result::Failure(EINVAL);

    auto sm = shm::FindById(shmid);
    if (sm == nullptr) return Result::Failure(EINVAL);

    void* ptr;
    if (auto result = shm::Map(*sm, ptr); result.IsFailure()) return result;
    *out = ptr;
    return Result::Success();
}
