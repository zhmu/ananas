#ifndef __SYS_RESOURCE_H__
#define __SYS_RESOURCE_H__

#include <sys/time.h>
#include <ananas/_types/id.h>
#include <sys/cdefs.h>

#define PRIO_PROCESS 0
#define PRIO_PGRP 1
#define PRIO_USER 2

typedef unsigned int rlim_t;

#define RLIM_INFINITY ((rlim_t)-1)
#define RLIM_SAVED_MAX RLIM_INFINITY
#define RLIM_SAVED_CUR RLIM_INFINITY

#define RUSAGE_SELF 0
#define RUSAGE_CHILDREN 1

#define RLIMIT_CORE 0
#define RLIMIT_CPU 1
#define RLIMIT_DATA 2
#define RLIMIT_FSIZE 3
#define RLIMIT_NOFILE 4
#define RLIMIT_STACK 5
#define RLIMIT_AS 6

struct rlimit {
    rlim_t rlim_cur;
    rlim_t rlim_max;
};

struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
};

__BEGIN_DECLS

int getpriority(int, id_t);
int getrlimit(int, struct rlimit*);
int getrusage(int, struct rusage*);
int setpriority(int, id_t, int);
int setrlimit(int, const struct rlimit*);

__END_DECLS

#endif /* __SYS_RESOURCE_H__ */
