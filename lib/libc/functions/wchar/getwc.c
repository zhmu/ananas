/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2020 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <wchar.h>
#include <stdio.h>

wint_t getwc(FILE* stream)
{
    int ch = getc(stream);
    if (ch == EOF) return WEOF;
    return (wint_t)ch;
}
