#include <sys/time.h>
#include <time.h>

int gettimeofday(struct timeval* tp, void* tz)
{
    tp->tv_sec = time(0);
    tp->tv_usec = 0;
    return 0;
}
