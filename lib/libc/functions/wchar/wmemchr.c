/* wmemchr( const void *, int, size_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <wchar.h>

wchar_t * wmemchr( const wchar_t * p, wchar_t c, size_t n )
{
    while ( n-- )
    {
        if ( *p == c )
        {
            return (wchar_t*) p;
        }
        ++p;
    }
    return NULL;
}
