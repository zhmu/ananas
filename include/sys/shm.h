/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/ipc.h>

#define IPC_STAT 1
#define IPC_SET 2
#define IPC_RMID 3

struct shmid_ds {
};

__BEGIN_DECLS

void* shmat(int shmid, const void* shmaddr, int shmflg);
int shmctl(int shmid, int cmd, struct shmid_ds* buf);
int shmdt(const void* shmaddr);
int shmget(key_t key, size_t size, int shmflg);

__END_DECLS
