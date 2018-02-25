/* strcpy( char *, const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>

char * strcpy( char * _PDCLIB_restrict s1, const char * _PDCLIB_restrict s2 )
{
    char * rc = s1;
    while ( ( *s1++ = *s2++ ) );
    return rc;
}
