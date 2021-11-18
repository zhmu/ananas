/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>
#include <ananas/flags.h>

#define O_ACCMODE (O_RDONLY | O_WRONLY | O_RDWR)

__BEGIN_DECLS

int creat(const char*, mode_t);
int open(const char*, int, ...);
int fcntl(int fildes, int cmd, ...);

__END_DECLS
