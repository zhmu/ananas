#include <time.h>

struct tm* localtime_r(const time_t* timer, struct tm* tm)
{
    /* XXX We don't support timezones */
    return gmtime_r(timer, tm);
}

struct tm* localtime(const time_t* timer)
{
    static struct tm shared_tm;
    return localtime_r(timer, &shared_tm);
}
