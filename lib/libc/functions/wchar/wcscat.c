/* wcscat( wchar_t *, const wchar_t * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <wchar.h>

wchar_t * wcscat( wchar_t * _PDCLIB_restrict s1,
                  const wchar_t * _PDCLIB_restrict s2 )
{
    wchar_t * rc = s1;
    if ( *s1 )
    {
        while ( *++s1 );
    }
    while ( (*s1++ = *s2++) );
    return rc;
}
