/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <time.h>
#include <stdio.h>

char* asctime(const struct tm* tm)
{
    static char result[64];
    strftime(result, sizeof(result) - 1, "%a %b %e %H:%M:%S\n", tm);
    return result;
}
