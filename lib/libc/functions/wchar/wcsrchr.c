/* wcsrchr( const wchar_t *, wchar_t );

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <wchar.h>
#include <stddef.h>

wchar_t* wcsrchr(const wchar_t* haystack, wchar_t needle)
{
    wchar_t* found = NULL;
    while (*haystack) {
        if (*haystack == needle)
            found = (wchar_t*)haystack;
        haystack++;
    }
    return found;
}
