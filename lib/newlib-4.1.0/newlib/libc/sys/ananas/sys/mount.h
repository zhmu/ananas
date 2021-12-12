/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __SYS_MOUNT_H__
#define __SYS_MOUNT_H__

#include <sys/cdefs.h>
#include <ananas/mount.h>

__BEGIN_DECLS

int getmntinfo(struct statfs** mntbufp, int flags);

int statfs(const char* path, struct statfs* buf);
int fstatfs(int fildes, struct statfs* buf);

/* Ananas extensions */
int mount(const char* type, const char* source, const char* dir, int flags);
int unmount(const char* dir, int flags);

__END_DECLS

#endif /* __SYS_MOUNT_H__ */
