/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>

#define IPC_CREAT 1
#define IPC_EXCL 2
#define IPC_NOWAIT 4

#define IPC_PRIVATE 0

struct ipc_perm {
    __uid_t uid;
    __gid_t gid;
    __uid_t cuid;
    __gid_t cgid;
    __mode_t mode;
};
