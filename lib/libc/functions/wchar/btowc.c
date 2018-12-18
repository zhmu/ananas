/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <wchar.h>

wint_t btowc(int c)
{
    wchar_t w;
    char ch = (char)c;
    if (mbrtowc(&w, &ch, 1, NULL) > WEOF)
        return WEOF;
    return w;
}
