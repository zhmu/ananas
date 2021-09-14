/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <sys/shm.h>
#include <ananas/syscalls.h>
#include "_map_statuscode.h"

int shmget(key_t key, size_t size, int shmflg)
{
    statuscode_t status = sys_shmget(key, size, shmflg);
    return map_statuscode(status);
}
