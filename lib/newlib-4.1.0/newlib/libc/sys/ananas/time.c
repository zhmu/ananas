#include <time.h>

time_t time(time_t* t)
{
    int seconds = -1;

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        seconds = ts.tv_sec;
        if (t != NULL)
            *t = seconds;
    }
    return seconds;
}
