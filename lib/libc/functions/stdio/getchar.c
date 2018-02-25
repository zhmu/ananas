/* getchar( void )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include "_PDCLIB_io.h"

// Testing covered by ftell.c
int _PDCLIB_getchar_unlocked( void )
{
    return _PDCLIB_fgetc_unlocked( stdin );
}

int getchar_unlocked( void )
{
	return _PDCLIB_getchar_unlocked( );
}

int getchar( void )
{
    return fgetc( stdin );
}
