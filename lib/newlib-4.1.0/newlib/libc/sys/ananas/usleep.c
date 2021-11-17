/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <time.h>
#include <unistd.h>

int usleep(useconds_t usec)
{
    struct timespec req, rem;
    req.tv_sec = usec / 1000000;
    req.tv_nsec = (usec % 1000000) * 1000;
    return nanosleep(&req, NULL);
}
