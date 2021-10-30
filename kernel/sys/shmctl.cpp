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

Result sys_shmctl(int shmid, int cmd, struct shmid_ds* buf)
{
    // XXX for now
    if (cmd != IPC_RMID) return Result::Failure(EINVAL);

    auto sm = shm::FindById(shmid);
    if (sm == nullptr) return Result::Failure(EINVAL);
    sm->destroyed = true;
    sm->Deref();
    return Result::Success();
}
