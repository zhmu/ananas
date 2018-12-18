/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <sys/times.h>
#include <_posix/todo.h>

clock_t times(struct tms* buffer)
{
    TODO;

    buffer->tms_utime = 0;
    buffer->tms_stime = 0;
    buffer->tms_cutime = 0;
    buffer->tms_cstime = 0;
    return 0;
}
