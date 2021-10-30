/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>

/* open() */
#define O_CREAT (1 << 0)
#define O_RDONLY (1 << 1)
#define O_WRONLY (1 << 2)
#define O_RDWR (1 << 3)
#define O_APPEND (1 << 4)
#define O_EXCL (1 << 5)
#define O_TRUNC (1 << 6)
#define O_CLOEXEC (1 << 7)
#define O_NONBLOCK (1 << 8)
#define O_NOCTTY (1 << 9)

/* Internal use only */
#define O_DIRECTORY (1 << 31)

/* fcntl() */
#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4

#define FD_CLOEXEC 1

__BEGIN_DECLS

int creat(const char*, mode_t);
int open(const char*, int, ...);
int fcntl(int fildes, int cmd, ...);

__END_DECLS
