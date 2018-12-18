/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <sched.h>
#include <errno.h>

int sched_rr_get_interval(pid_t pid, struct timespec* interval)
{
    errno = ENOSYS;
    return -1;
}
