/* wcschr( const wchar_t *, wchar_t );

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <wchar.h>
#include <stddef.h>

wchar_t *wcschr(const wchar_t * haystack, wchar_t needle)
{
    while(*haystack) {
        if(*haystack == needle) return (wchar_t*) haystack;
        haystack++;
    }
    return NULL;
}
