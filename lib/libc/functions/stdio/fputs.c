/* fputs( const char *, FILE * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include "_PDCLIB_io.h"

int _PDCLIB_fputs_unlocked( const char * _PDCLIB_restrict s,
                    FILE * _PDCLIB_restrict stream )
{
    if ( _PDCLIB_prepwrite( stream ) == EOF )
    {
        return EOF;
    }
    while ( *s != '\0' )
    {
        /* Unbuffered and line buffered streams get flushed when fputs() does
           write the terminating end-of-line. All streams get flushed if the
           buffer runs full.
        */
        stream->buffer[ stream->bufidx++ ] = *s;
        if ( ( stream->bufidx == stream->bufsize ) ||
             ( ( stream->status & _IOLBF ) && *s == '\n' )
           )
        {
            if ( _PDCLIB_flushbuffer( stream ) == EOF )
            {
                return EOF;
            }
        }
        ++s;
    }
    if ( stream->status & _IONBF )
    {
        if ( _PDCLIB_flushbuffer( stream ) == EOF )
        {
            return EOF;
        }
    }
    return 0;
}

int fputs_unlocked( const char * _PDCLIB_restrict s,
                    FILE * _PDCLIB_restrict stream )
{
    return _PDCLIB_fputs_unlocked( s, stream );
}

int fputs( const char * _PDCLIB_restrict s,
           FILE * _PDCLIB_restrict stream )
{
    _PDCLIB_flockfile( stream );
    int r = _PDCLIB_fputs_unlocked( s, stream );
    _PDCLIB_funlockfile( stream );
    return r;
}
