/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>

struct tms {
    __clock_t   tms_utime;  /* User CPU time */
    __clock_t   tms_stime;  /* System CPU time */
    __clock_t   tms_cutime; /* User CPU time of terminated child processes */
    __clock_t   tms_cstime; /* System CPU time of terminated child processes */
};
