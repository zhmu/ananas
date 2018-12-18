/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <wchar.h>

size_t mbrlen(const char* s, size_t n, mbstate_t* ps)
{
    mbstate_t internal;
    return mbrtowc(NULL, s, n, ps != NULL ? ps : &internal);
}
