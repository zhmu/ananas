/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>

#define FD_BITS_PER_FDS (sizeof(long) * 8)

#define FD_CLR(fd, fdset) ((fdset)->fds_bits[(fd) / FD_BITS_PER_FDS] &= ~(1L << ((fd) % FD_BITS_PER_FDS)))
#define FD_ISSET(fd, fdset) ((fdset)->fds_bits[(fd) / FD_BITS_PER_FDS] & (1L << ((fd) % FD_BITS_PER_FDS)))
#define FD_SET(fd, fdset) ((fdset)->fds_bits[(fd) / FD_BITS_PER_FDS] |= (1L << ((fd) % FD_BITS_PER_FDS)))
#define FD_ZERO(fdset) do { char* __p = (char*)(fdset); for(size_t __n = 0; __n < sizeof(fd_set); ++__n) { *__p++ = 0; } } while(0)

#define FD_SETSIZE 64 /* XXX */
