/* $Id: fseek.c 382 2009-10-26 22:27:59Z solar $ */

/* fseek( FILE *, long offset, int )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>

#ifndef REGTEST

#include <_PDCLIB/_PDCLIB_glue.h>

int fseek( struct _PDCLIB_file_t * stream, long offset, int whence )
{
    if ( stream->status & _PDCLIB_FWRITE )
    {
        if ( _PDCLIB_flushbuffer( stream ) == EOF )
        {
            return EOF;
        }
    }
    stream->status &= ~ _PDCLIB_EOFFLAG;
    if ( stream->status & _PDCLIB_FRW )
    {
        stream->status &= ~ ( _PDCLIB_FREAD | _PDCLIB_FWRITE );
    }
    return ( _PDCLIB_seek( stream, offset, whence ) == offset ) ? 0 : EOF;
}

#endif

#ifdef TEST
#include <_PDCLIB_test.h>

int main( void )
{
    /* Testing covered by ftell.c */
    return TEST_RESULTS;
}

#endif
