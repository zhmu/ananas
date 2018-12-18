/* wcsncmp( const wchar_t *, const wchar_t *, size_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <wchar.h>

int wcsncmp(const wchar_t* s1, const wchar_t* s2, size_t n)
{
    while (*s1 && n && (*s1 == *s2)) {
        ++s1;
        ++s2;
        --n;
    }
    if (n == 0) {
        return 0;
    } else {
        return (*(wchar_t*)s1 - *(wchar_t*)s2);
    }
}
