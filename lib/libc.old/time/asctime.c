#include <time.h>
#include <stdio.h>

static char asctime_result[26];

static const char wday_name[7][3] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char month_name[12][3] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
	"Aug", "Sep", "Oct", "Nov", "Dec"
};

char* asctime(const struct tm* timeptr)
{
	snprintf(asctime_result, sizeof(asctime_result),
	 "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n",
	 wday_name[timeptr->tm_wday], month_name[timeptr->tm_mon],
	 timeptr->tm_mday,
	 timeptr->tm_hour, timeptr->tm_min, timeptr->tm_sec,
	 1900 + timeptr->tm_year);
	return asctime_result;
}
