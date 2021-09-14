/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __TIMEVAL_T_DEFINED
#define __TIMEVAL_T_DEFINED

#include <ananas/_types/suseconds.h>
#include <ananas/_types/time.h>

struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};

#endif /* __TIMEVAL_T_DEFINED */
