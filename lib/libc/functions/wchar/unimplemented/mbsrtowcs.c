/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <errno.h>
#include <wchar.h>

size_t mbsrtowcs(wchar_t* dst, const char** src, size_t len, mbstate_t* ps)
{
    errno = EILSEQ;
    return (size_t)-1;
}
