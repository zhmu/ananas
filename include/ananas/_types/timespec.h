#ifndef __TIMESPEC_T_DEFINED
#define __TIMESPEC_T_DEFINED

#include <ananas/_types/time.h>

struct timespec {
	time_t	tv_sec;			/* seconds */
	long	tv_nsec;		/* nanoseconds */
};

#endif /* __TIMESPEC_T_DEFINED */
