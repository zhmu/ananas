/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include <sys/shm.h>
#include "kernel/result.h"
#include "kernel/shm.h"
#include "kernel-md/param.h"
#include "syscall.h"

Result sys_shmget(key_t key, size_t size, int shmflg)
{
    // XXX For now
    if (key != IPC_PRIVATE) return Result::Failure(EINVAL);
    if ((shmflg & IPC_CREAT) == 0) return Result::Failure(EINVAL);

    if ((size & (PAGE_SIZE - 1)) != 0) return Result::Failure(EINVAL);

    auto sm = shm::Allocate(size);
    return Result::Success(sm->shm_id);
}
