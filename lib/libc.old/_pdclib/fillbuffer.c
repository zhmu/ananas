/* $Id: fillbuffer.c 366 2009-09-13 15:14:02Z solar $ */

/* _PDCLIB_fillbuffer( struct _PDCLIB_file_t * stream )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

/* This is an example implementation of _PDCLIB_fillbuffer() fit for
   use with POSIX kernels.
*/

#include <stdio.h>
#include <unistd.h>

#ifndef REGTEST
#include <_PDCLIB/_PDCLIB_glue.h>

int _PDCLIB_fillbuffer( struct _PDCLIB_file_t * stream )
{
    /* No need to handle buffers > INT_MAX, as PDCLib doesn't allow them */
    ssize_t rc = read( stream->handle, stream->buffer, stream->bufsize );
    if ( rc > 0 )
    {
        /* Reading successful. */
        if ( ! ( stream->status & _PDCLIB_FBIN ) )
        {
            /* TODO: Text stream conversion here */
        }
        stream->pos.offset += rc;
        stream->bufend = rc;
        stream->bufidx = 0;
        return 0;
    }
    if ( rc < 0 )
    {
        /* note that read() sets errno for us */
        stream->status |= _PDCLIB_ERRORFLAG;
        return EOF;
    }
    /* End-of-File */
    stream->status |= _PDCLIB_EOFFLAG;
    return EOF;
}

#endif

#ifdef TEST
#include <_PDCLIB/_PDCLIB_test.h>

int main( void )
{
    /* Testing covered by ftell.c */
    return TEST_RESULTS;
}

#endif

