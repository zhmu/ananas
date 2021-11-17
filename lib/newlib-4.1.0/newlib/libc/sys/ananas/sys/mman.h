/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <sys/types.h>
#include <sys/cdefs.h>

#define PROT_READ (1 << 0)
#define PROT_WRITE (1 << 1)
#define PROT_EXEC (1 << 2)
#define PROT_NONE (1 << 3)

#define MAP_SHARED (1 << 0)
#define MAP_PRIVATE (1 << 1)
#define MAP_FIXED (1 << 2)
#define MAP_ANON (1 << 3) /* not part of POSIX */
#define MAP_ANONYMOUS MAP_ANON

#define MAP_FAILED ((void*)-1)

#define MS_ASYNC (1 << 0)
#define MS_SYNC (1 << 1)
#define MS_INVALIDATE (1 << 2)

#define MCL_CURRENT (1 << 0)
#define MCL_FUTURE (1 << 1)

__BEGIN_DECLS

void* mmap(void*, size_t, int, int, int, off_t);
int munmap(void*, size_t);
int mprotect(void*, size_t, int);

__END_DECLS
