/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types/timeval.h>

struct itimerval {
    struct timeval it_interval;         /* timer interval */
    struct timeval it_value;            /* current value */
};

