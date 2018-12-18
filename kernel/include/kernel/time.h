#ifndef __ANANAS_TIME_H__
#define __ANANAS_TIME_H__

#include <ananas/types.h>

#define CLOCK_MONOTONIC 0
#define CLOCK_REALTIME 1
#define CLOCK_SECONDS 2

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

void delay(int ms);

namespace time
{
    unsigned int GetPeriodicyInHz();

    void OnTick();
    tick_t GetTicks();

    /*
     * Tick counter comparison functions.
     */
    inline bool IsTickBefore(tick_t a, tick_t b) { return ((int64_t)b - (int64_t)a) > 0; }

    inline bool IsTickAfter(tick_t a, tick_t b) { return IsTickBefore(b, a); }

    void SetTime(const struct tm& tm);
    void SetTime(const struct timespec& ts);
    struct timespec GetTime();

} // namespace time

#endif /* __ANANAS_TIME_H__ */
