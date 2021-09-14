/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <machine/_types.h>
#include <ananas/_types/uid.h>
#include <ananas/_types/gid.h>
#include <ananas/_types/mode.h>

#ifndef __SYS_IPC_H__
#define __SYS_IPC_H__

#define IPC_CREAT 1
#define IPC_EXCL 2
#define IPC_NOWAIT 4

#define IPC_PRIVATE 0

struct ipc_perm {
    uid_t uid;
    gid_t gid;
    uid_t cuid;
    gid_t cgid;
    mode_t mode;
};

#endif /* __SYS_IPC_H__ */
