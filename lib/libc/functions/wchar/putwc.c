/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2020 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <wchar.h>
#include <stdio.h>

wint_t putwc(wchar_t wc, FILE* stream)
{
    int r = putc((char)wc, stream);
    return r == EOF ? WEOF : r;
}
