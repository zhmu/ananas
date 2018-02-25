/* gets( char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include <stdint.h>
#include "_PDCLIB_io.h"

char * gets( char * s )
{
    _PDCLIB_flockfile( stdin );
    if ( _PDCLIB_prepread( stdin ) == EOF )
    {
        _PDCLIB_funlockfile( stdin );
        return NULL;
    }
    char * dest = s;

    dest += _PDCLIB_getchars( dest, SIZE_MAX, '\n', stdin );
    _PDCLIB_funlockfile( stdin );

    if(*(dest - 1) == '\n') {
        *(--dest) = '\0';
    } else {
        *dest = '\0';
    }

    return ( dest == s ) ? NULL : s;
}
