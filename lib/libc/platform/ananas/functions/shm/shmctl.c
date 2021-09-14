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

int shmctl(int shmid, int cmd, struct shmid_ds* buf)
{
    statuscode_t status = sys_shmctl(shmid, cmd, buf);
    return map_statuscode(status);
}
