/* $Id: fopen.c 366 2009-09-13 15:14:02Z solar $ */

/* fopen( const char *, const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include <stdlib.h>

#ifndef REGTEST
#include <_PDCLIB/_PDCLIB_glue.h>

extern struct _PDCLIB_file_t * _PDCLIB_filelist;

struct _PDCLIB_file_t * fdopen( int fildes, const char * _PDCLIB_restrict mode )
{
    struct _PDCLIB_file_t * rc;
    if ( mode == NULL )
    {
        /* Mode invalid */
        return NULL;
    }
    if ( ( rc = calloc( 1, sizeof( struct _PDCLIB_file_t ) ) ) == NULL )
    {
        /* no memory for another FILE */
        return NULL;
    }
    if ( ( rc->status = _PDCLIB_filemode( mode ) ) == 0 ) 
    {
        /* invalid mode */
        free( rc );
        return NULL;
    }
    rc->handle = fildes;
    /* Adding to list of open files */
    rc->next = _PDCLIB_filelist;
    _PDCLIB_filelist = rc;
    /* Setting buffer, and mark as internal. TODO: Check for unbuffered */
    if ( ( rc->buffer = malloc( BUFSIZ ) ) == NULL )
    {
        free( rc );
        return NULL;
    }
    if ( ( rc->ungetbuf = malloc( _PDCLIB_UNGETCBUFSIZE ) ) == NULL )
    {
       free( rc->buffer );
       free( rc );
       return NULL;
    }
    rc->bufsize = BUFSIZ;
    rc->bufidx = 0;
    rc->ungetidx = 0;
    /* Setting buffer to _IOLBF because "when opened, a stream is fully
       buffered if and only if it can be determined not to refer to an
       interactive device."
    */
    rc->status |= _PDCLIB_LIBBUFFER | _IOLBF;
    /* TODO: Setting mbstate */
    return rc;
}

#endif
