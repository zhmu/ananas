/* putc( int, FILE * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include "_PDCLIB_io.h"

/* Testing covered by ftell.c */
int _PDCLIB_putc_unlocked( int c, FILE * stream )
{
    return _PDCLIB_fputc_unlocked( c, stream );
}

int putc_unlocked( int c, FILE * stream )
{
	return _PDCLIB_putc_unlocked( c, stream );
}

int putc( int c, FILE * stream )
{
    return fputc( c, stream );
}
