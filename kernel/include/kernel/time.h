/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <cstdint>
#include <sys/time.h>

#define CLOCK_MONOTONIC 0
#define CLOCK_REALTIME 1
#define CLOCK_SECONDS 2

// XXX shouldn't this be in sys/time.h ?
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
    struct timespec GetTimeSinceBoot();

    tick_t TimevalToTicks(const timeval& tv);
    tick_t TimespecToTicks(const timespec& ts);

} // namespace time
