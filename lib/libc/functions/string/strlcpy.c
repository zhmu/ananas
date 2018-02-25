/* strlcpy( char *, const char *, size_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>

#pragma weak strlcpy = _PDCLIB_strlcpy
size_t _PDCLIB_strlcpy(
    char *restrict dst,
    const char *restrict src,
    size_t dstsize);

size_t _PDCLIB_strlcpy(
    char *restrict dst,
    const char *restrict src,
    size_t dstsize)
{
    size_t needed = 0;
    while(needed < dstsize && (dst[needed] = src[needed]))
        needed++;

    while(src[needed++]);

    if (needed > dstsize && dstsize)
      dst[dstsize - 1] = 0;

    return needed;
}
