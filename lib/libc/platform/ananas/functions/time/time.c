#include <time.h>

#ifndef REGTEST
time_t time(time_t* t)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) < 0)
        ts.tv_sec = -1;

    if(t != NULL)
        *t = ts.tv_sec;
    return ts.tv_sec;
}
#endif

#ifdef TEST
#include "_PDCLIB_test.h"

int main( void )
{
    return TEST_RESULTS;
}

#endif
