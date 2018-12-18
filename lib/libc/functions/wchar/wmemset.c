/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <wchar.h>

wchar_t* wmemset(wchar_t* ws, wchar_t wc, size_t n)
{
    wchar_t* p = ws;
    for (/* nothing */; n > 0; n--)
        *p++ = wc;
    return ws;
}
