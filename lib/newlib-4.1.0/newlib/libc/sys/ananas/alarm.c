/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <sys/time.h>
#include <string.h>

unsigned int alarm(unsigned int seconds)
{
    struct itimerval it_new, it_prev;
    memset(&it_new, 0, sizeof(it_new));
    it_new.it_value.tv_sec = seconds;
    if (setitimer(ITIMER_REAL, &it_new, &it_prev) == 0)
        return it_prev.it_value.tv_sec + it_prev.it_value.tv_usec ? 1 : 0; // always round usec up
    return 0;
}
