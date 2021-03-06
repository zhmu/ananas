/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <unistd.h>
#include <machine/param.h>
#include <limits.h>

long sysconf(int name)
{
    switch (name) {
        case _SC_PAGESIZE:
            return PAGE_SIZE;
        case _SC_CLK_TCK:
            return 1; /* TODO */
        case _SC_ARG_MAX:
            return _POSIX_ARG_MAX;
        default:
            return -1;
    }
}

/* vim:set ts=2 sw=2: */
