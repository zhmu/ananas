/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __SYS_TIME_H__
#define __SYS_TIME_H__

#include <sys/types.h>
#include <ananas/_types/suseconds.h>
#include <sys/cdefs.h>

struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};

#include <sys/select.h>

struct itimerval {
    struct timeval it_interval;
    struct timeval it_value;
};

__BEGIN_DECLS

int gettimeofday(struct timeval* tp, void* tz);

__END_DECLS

#endif /* __SYS_TIME_H__ */
