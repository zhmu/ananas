#include <sys/types.h>

#ifndef __SYS_SELECT_H__
#define __SYS_SELECT_H__

struct timeval;

typedef struct {
	long fds_bits;
} fd_set;

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* errorfds, struct timeval* timeout);

#define FD_CLR(fd, fdset)
#define FD_ISSET(fd, fdset) (0)
#define FD_SET(fd, fdset)
#define FD_ZERO(fdset)

#define FD_SETSIZE 64 /* XXX */

#endif /* __SYS_SELECT_H__ */
