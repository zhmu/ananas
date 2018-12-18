/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __SYS_STAT_H__
#define __SYS_STAT_H__

#include <ananas/stat.h> /* for struct stat and friends */
#include <sys/cdefs.h>

__BEGIN_DECLS

int chmod(const char*, mode_t);
int fchmod(int, mode_t);
int fstat(int, struct stat*);
int lstat(const char*, struct stat*);
int mkdir(const char*, mode_t);
int mkfifo(const char*, mode_t);
int mknod(const char*, mode_t, dev_t);
int stat(const char*, struct stat*);
mode_t umask(mode_t cmask);

__END_DECLS

#endif /* __SYS_STAT_H__ */
