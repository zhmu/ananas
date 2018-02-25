/* putchar( int )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include "_PDCLIB_io.h"

int _PDCLIB_putchar_unlocked( int c )
{
    return _PDCLIB_fputc_unlocked( c, stdout );
}

int putchar_unlocked( int c )
{
    return _PDCLIB_putchar_unlocked( c );
}

// Testing covered by ftell.cpp
int putchar( int c )
{
    return fputc( c, stdout );
}
