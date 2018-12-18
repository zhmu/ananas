/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef _POSIX_TODO_H_
#define _POSIX_TODO_H_

#include <stdio.h> /* for printf() */
#include <errno.h> /* for errno :-) */

#undef TODO_VERBOSE

#ifdef TODO_VERBOSE
#define TODO                                           \
    do {                                               \
        printf("%s: not implemented yet\n", __func__); \
        errno = ENOSYS;                                \
    } while (0)
#else
#define TODO            \
    do {                \
        errno = ENOSYS; \
    } while (0)
#endif

#endif /* _POSIX_TODO_H_ */
