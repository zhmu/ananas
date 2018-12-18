/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

int wctob(wint_t c)
{
    char out[MB_LEN_MAX];
    if (c == WEOF || wctomb(out, c) != 1)
        return EOF;
    return out[0];
}
