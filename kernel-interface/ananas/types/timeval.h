/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

struct timeval {
    __time_t        tv_sec;     /* seconds */
    __suseconds_t   tv_usec;    /* microseconds */
};
