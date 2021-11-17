/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <stdio.h>
#include <errno.h>

#undef TODO_VERBOSE

#ifdef TODO_VERBOSE
#define TODO                                           \
    do {                                               \
        fprintf(stderr, "%s: not implemented\n", __func__); \
        errno = ENOSYS;                                \
    } while (0)
#else
#define TODO            \
    do {                \
        errno = ENOSYS; \
    } while (0)
#endif
