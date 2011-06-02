/* $Id: ftell.c 366 2009-09-13 15:14:02Z solar $ */

/* ftello( FILE * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include <errno.h>
#include <limits.h>

off_t ftello( struct _PDCLIB_file_t * stream )
{
    /* TODO: A bit too fuzzy in the head now. stream->ungetidx should be in here
             somewhere.
    */
    if ( stream->pos.offset > ( LONG_MAX - stream->bufidx ) )
    {
        /* integer overflow */
        _PDCLIB_errno = EINVAL;
        return -1;
    }
    /* Position of start-of-buffer, plus:
       - buffered, unwritten content (for output streams), or
       - already-parsed content from buffer (for input streams)
    */
    return (off_t)( stream->pos.offset + stream->bufidx - stream->ungetidx );
}
