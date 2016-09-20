#ifndef __SYS_RESOURCE_H__
#define __SYS_RESOURCE_H__

#include <sys/time.h>

typedef unsigned int rlim_t;

struct rlimit {
	rlim_t rlim_cur;
	rlim_t rlim_max;
};

struct rusage {
	struct timeval ru_utime;
	struct timeval ru_stime;
};

#if 0
int getpriority(int, id_t);
int getrlimit(int, struct rlimit*);
int getrusage(int, struct rusage*);
int setpriority(int, id_t, int);
int setrlimit(int, const struct rlimit*);
#endif

#endif /* __SYS_RESOURCE_H__ */
