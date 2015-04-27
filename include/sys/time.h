#ifndef __SYS_TIME_H__
#define __SYS_TIME_H__

#include <sys/types.h>

struct timeval {
	time_t		tv_sec;
	suseconds_t	tv_usec;
};

#include <sys/select.h>

struct itimerval {
	struct timeval	it_interval;
	struct timeval	it_value;
};

int gettimeofday(struct timeval* tp, void* tz);

#endif /* __SYS_TIME_H__ */
