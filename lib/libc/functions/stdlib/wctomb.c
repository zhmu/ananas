/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <stdlib.h>
#include <wchar.h>

int wctomb(char* s, wchar_t wc)
{
    if (s == NULL) {
        return 0;
    }

    mbstate_t mb;
    size_t r = wcrtomb(s, wc, &mb);
    if (r == (size_t)-1)
        return -1;
    return (int)r;
}
