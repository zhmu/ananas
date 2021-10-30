/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include <ananas/errno.h>
#include "kernel/result.h"
#include "kernel/shm.h"
#include "syscall.h"

Result sys_shmdt(const void* shmaddr)
{
    return shm::Unmap(shmaddr);
}
