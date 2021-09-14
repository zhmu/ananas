/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <sys/types.h>
#include <sys/cdefs.h>

#ifndef __SYS_SELECT_H__
#define __SYS_SELECT_H__

#include <ananas/_types/timeval.h>
#include <ananas/_types/fdset.h>

__BEGIN_DECLS

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* errorfds, struct timeval* timeout);

__END_DECLS

#define FD_BITS_PER_FDS (sizeof(long) * 8)

#define FD_CLR(fd, fdset) ((fdset)->fds_bits[(fd) / FD_BITS_PER_FDS] &= ~(1L << ((fd) % FD_BITS_PER_FDS)))
#define FD_ISSET(fd, fdset) ((fdset)->fds_bits[(fd) / FD_BITS_PER_FDS] & (1L << ((fd) % FD_BITS_PER_FDS)))
#define FD_SET(fd, fdset) ((fdset)->fds_bits[(fd) / FD_BITS_PER_FDS] |= (1L << ((fd) % FD_BITS_PER_FDS)))
#define FD_ZERO(fdset) do { char* __p = (char*)(fdset); for(size_t __n = 0; __n < sizeof(fd_set); ++__n) { *__p++ = 0; } } while(0)

#define FD_SETSIZE 64 /* XXX */

#endif /* __SYS_SELECT_H__ */
