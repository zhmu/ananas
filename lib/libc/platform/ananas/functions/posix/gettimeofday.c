/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <sys/time.h>
#include <time.h>

int gettimeofday(struct timeval* tp, void* tz)
{
    tp->tv_sec = time(0);
    tp->tv_usec = 0;
    return 0;
}
