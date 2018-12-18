/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <stdlib.h>
#include <wchar.h>

int mbtowc(wchar_t* restrict pwc, const char* restrict s, size_t n)
{
    return (int)mbrtowc(pwc, s, n, NULL);
}
