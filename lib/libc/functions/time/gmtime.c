/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <time.h>
#include <stdint.h>

// SerialDayNumberToDate() and SerialDayNumberToWeekDay() are inspired by
// http://howardhinnant.github.io/date_algorithms.html, days_from_civil(), weekday_from_days()
static void SerialDayNumberToDate(time_t z, struct tm* tm)
{
    z += 719468;
    int32_t era = (z >= 0 ? z : z - 146096) / 146097;
    uint32_t dayOfEra = (uint32_t)(z - era * 146097); // 0 .. 146096
    uint32_t yearOfEra =
        (dayOfEra - dayOfEra / 1460 - dayOfEra / 36524 - dayOfEra / 146096) / 365; // 0 .. 399
    tm->tm_year = (int32_t)yearOfEra + era * 400;
    tm->tm_yday = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100); // 0 .. 365
    uint32_t mp = (5 * tm->tm_yday + 2) / 153;                                    // 0 .. 11
    tm->tm_mday = tm->tm_yday - (153 * mp + 2) / 5 + 1;                           // 1 .. 31
    tm->tm_mon = (mp + (mp < 10 ? 3 : -9)) - 1;                                   // 0 .. 11
    if (tm->tm_mon <= 1)
        tm->tm_year++;
    tm->tm_year -= 1900;
}

static void SerialDayNumberToWeekDay(time_t z, struct tm* tm)
{
    tm->tm_wday = (z >= -4) ? (z + 4) % 7 : (z + 5) % 7 + 6; // 0 .. 6
}

struct tm* gmtime_r(const time_t* timer, struct tm* tm)
{
    time_t t = *timer;
    tm->tm_sec = t % 60;
    t /= 60;
    tm->tm_min = t % 60;
    t /= 60;
    tm->tm_hour = t % 24;
    t /= 24;
    SerialDayNumberToDate(t, tm);
    SerialDayNumberToWeekDay(t, tm);
    tm->tm_isdst = 0; // TODO
    return tm;
}

struct tm* gmtime(const time_t* timer)
{
    static struct tm shared_tm;
    return gmtime_r(timer, &shared_tm);
}
