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

struct timeval;

typedef struct {
    long fds_bits;
} fd_set;

__BEGIN_DECLS

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* errorfds, struct timeval* timeout);

__END_DECLS

#define FD_CLR(fd, fdset)
#define FD_ISSET(fd, fdset) (0)
#define FD_SET(fd, fdset)
#define FD_ZERO(fdset)

#define FD_SETSIZE 64 /* XXX */

#endif /* __SYS_SELECT_H__ */
