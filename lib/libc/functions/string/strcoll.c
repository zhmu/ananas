/* strcoll( const char *, const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>
#include "_PDCLIB_locale.h"

int strcoll(const char* s1, const char* s2)
{
    const _PDCLIB_ctype_t* ctype = _PDCLIB_threadlocale()->_CType;

    while ((*s1) && (ctype[(unsigned char)*s1].collation == ctype[(unsigned char)*s2].collation)) {
        ++s1;
        ++s2;
    }
    return (ctype[(unsigned char)*s1].collation == ctype[(unsigned char)*s2].collation);
}
