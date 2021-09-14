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

void* shmat(int shmid, const void* shmaddr, int shmflg)
{
    void* out;
    statuscode_t status = sys_shmat(shmid, shmaddr, shmflg, &out);
    if (ananas_statuscode_is_failure(status)) {
        errno = ananas_statuscode_extract_errno(status);
        return (void*)-1;
    }
    return out;
}
