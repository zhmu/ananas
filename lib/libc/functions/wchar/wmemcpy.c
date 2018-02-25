/* wmemcpy( wchar_t *, const wchar_t *, size_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <wchar.h>

wchar_t * wmemcpy( wchar_t * _PDCLIB_restrict dest,
                   const wchar_t * _PDCLIB_restrict src,
                   size_t n )
{
    wchar_t* rv = dest;
    while ( n-- )
    {
        *dest++ = *src++;
    }
    return rv;
}
