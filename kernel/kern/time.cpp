/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/time.h"
#include "kernel/lock.h"
#include "kernel/pcpu.h"
#include "kernel/schedule.h"
#include "kernel/thread.h"

namespace time
{
    namespace
    {
        // XXX Maybe using a spinlock here isn't such a good idea - we could benefit much more
        //     from atomic add/subtract/compare...
        Spinlock time_lock;
        tick_t ticks = 0;
        struct timespec time_boot;
        struct timespec time_current;

        // DateToSerialDayNumber() is inspired by
        // http://howardhinnant.github.io/date_algorithms.html, days_from_civil()
        time_t DateToSerialDayNumber(const struct tm& tm)
        {
            int32_t d = tm.tm_mday;
            int32_t m = tm.tm_mon;
            int32_t y = tm.tm_year;
            if (m <= 2)
                y--;

            int32_t era = (y >= 0 ? y : y - 399) / 400;
            uint32_t yearOfEra = static_cast<uint32_t>(y - era * 400);           // 0 .. 399
            uint32_t dayOfYear = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; // 0 .. 365
            uint32_t dayOfEra =
                yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear; // 0 .. 1460

            return era * 146097 + static_cast<int32_t>(dayOfEra) - 719468;
        }

        void TMtoTS(const struct tm& tm, struct timespec& ts)
        {
            ts.tv_sec = DateToSerialDayNumber(tm) * (24 * 60 * 60);
            ts.tv_sec += tm.tm_hour * (60 * 60);
            ts.tv_sec += tm.tm_min * 60;
            ts.tv_sec += tm.tm_sec;
            ts.tv_nsec = 0;
        }

        void AddTick(struct timespec& ts)
        {
            ts.tv_nsec += 1000000000 / GetPeriodicyInHz();
            while (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
        }
    } // unnamed namespace

    unsigned int GetPeriodicyInHz()
    {
        // XXX make me configurable in some way
        return 100;
    }

    tick_t GetTicks()
    {
        SpinlockUnpremptibleGuard g(time_lock);
        return ticks;
    }

    void SetTime(const struct tm& tm)
    {
        struct timespec ts;
        TMtoTS(tm, ts);
        SetTime(ts);
    }

    void SetTime(const struct timespec& ts)
    {
        SpinlockUnpremptibleGuard g(time_lock);
        time_current = ts;
    }

    struct timespec GetTime()
    {
        SpinlockUnpremptibleGuard g(time_lock);
        return time_current;
    }

    struct timespec GetTimeSinceBoot()
    {
        SpinlockUnpremptibleGuard g(time_lock);
        return time_boot;
    }

    void OnTick()
    {
        // This should only be called in the boot CPU

        // Increment system tick count
        {
            SpinlockUnpremptibleGuard g(time_lock);
            ticks++;

            // Update the timestamp - XXX we should synchronise every now and then with
            // the RTC. XXX we can use the TSC to get a much more accurate value than
            // this
            AddTick(time_boot);
            AddTick(time_current);
        }
    }

    tick_t TimevalToTicks(const timeval& tv)
    {
        const auto hz = GetPeriodicyInHz();
        const auto ticksPerMs = 1'000'000 / hz;
        tick_t t = tv.tv_sec * hz;
        t += tv.tv_usec / ticksPerMs;
        return t;
    }

    tick_t TimespecToTicks(const timespec& ts)
    {
        const auto hz = GetPeriodicyInHz();
        const auto ticksPerNs = 1'000'000'000 / hz;
        tick_t t = ts.tv_sec * hz;
        t += ts.tv_nsec / ticksPerNs;
        return t;
    }
} // namespace time
