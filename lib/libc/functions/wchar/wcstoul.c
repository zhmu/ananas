/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <wchar.h>
#include <stdlib.h>

unsigned long wcstoul(const wchar_t* ptr, wchar_t** endptr, int base)
{
    size_t len = wcstombs(NULL, ptr, 0);
    if (len == (size_t)-1) {
        if (endptr != NULL)
            *endptr = (wchar_t*)ptr;
        return 0;
    }

    char* buf = malloc(len + 1);
    if (buf == NULL) {
        if (endptr != NULL)
            *endptr = (wchar_t*)ptr;
        return 0;
    }

    wcstombs(buf, ptr, len);

    char* endp;
    unsigned long l = strtoul(buf, &endp, base);
    if (endptr != NULL) {
        *endptr = (wchar_t*)ptr + (endp - buf);
    }

    free(buf);
    return l;
}
