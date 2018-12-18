/* strxfrm( char *, const char *, size_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>
#include "_PDCLIB_locale.h"

size_t strxfrm(char* _PDCLIB_restrict s1, const char* _PDCLIB_restrict s2, size_t n)
{
    const _PDCLIB_ctype_t* ctype = _PDCLIB_threadlocale()->_CType;
    size_t len = strlen(s2);
    if (len < n) {
        /* Cannot use strncpy() here as the filling of s1 with '\0' is not part
           of the spec.
        */
        while (n-- && (*s1++ = ctype[(unsigned char)*s2++].collation))
            ;
    }
    return len;
}
