#include <sys/types.h>

#ifndef __SYS_TIME_H__
#define __SYS_TIME_H__

struct timeval {
	time_t		tv_sec;
	suseconds_t	tv_usec;
};

struct itimerval {
	struct timeval	it_interval;
	struct timeval	it_value;
};

typedef struct {
	long fds_bits;
} fd_set;

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* errorfds, struct timeval* timeout);

#define FD_CLR(fd, fdset)
#define FD_ISSET(fd, fdset) (0)
#define FD_SET(fd, fdset)
#define FD_ZERO(fdset)

#endif /* __SYS_TIME_H__ */
