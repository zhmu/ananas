/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <string.h>

char* strsep(char** stringp, const char* delim)
{
    char* ret = *stringp;
    if (*stringp == NULL)
        return ret;

    for (char* p = ret; *p != '\0'; /* nothing */) {
        // Find first match in delim
        char* match = strchr(delim, *p);
        if (match == NULL) {
            p++;
            continue;
        }

        *p = '\0';
        *stringp = p + 1;
        return ret;
    }

    *stringp = NULL;
    return ret;
}
