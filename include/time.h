#include <ananas/types.h>

#ifndef __TIME_H__
#define __TIME_H__

struct tm {
	int tm_sec;		/* seconds (0 - 59) */
	int tm_min;		/* minutes (0 - 59) */
	int tm_hour;		/* hours (0 - 23) */
	int tm_mday;		/* day of month (1 - 31) */
	int tm_mon;		/* month (0 - 11) */
	int tm_year;		/* year - 1900 */
	int tm_wday;		/* day of week (0 - 6; sunday = 0) */
	int tm_yday;		/* day of year (0 - 365) */
	int tm_isdst;		/* is summer time active? */
	char* tm_zone;		/* time zone name */
	long tm_gmtoff;		/* offset from UTC in seconds */
};

struct timespec {
	time_t	tv_sec;		/* Seconds */
	long	tv_nsec;	/* Nanoseconds */
};

void tzset(void);
time_t time(time_t* loc);
clock_t clock(void);
struct tm* gmtime(const time_t* clock);
struct tm* localtime(const time_t* clock);
time_t mktime(struct tm* tm);
double difftime(time_t time1, time_t time0);
size_t strftime(char* buf, size_t maxsize, const char* format, const struct tm* timeptr);
int nanosleep(const struct timespec* rqtp, struct timespec* rmtp);
char* ctime(const time_t* clock);

#endif /* __TIME_H__ */
