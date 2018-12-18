/* wcsncat( wchar_t *, const wchar_t *, size_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <wchar.h>

wchar_t* wcsncat(wchar_t* _PDCLIB_restrict s1, const wchar_t* _PDCLIB_restrict s2, size_t n)
{
    wchar_t* rc = s1;
    while (*s1) {
        ++s1;
    }
    while (n && (*s1++ = *s2++)) {
        --n;
    }
    if (n == 0) {
        *s1 = '\0';
    }
    return rc;
}
