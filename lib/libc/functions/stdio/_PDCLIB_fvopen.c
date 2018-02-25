/* _PDCLIB_fvopen( _PDCLIB_fd_t, _PDCLIB_fileops_t * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include "_PDCLIB_glue.h"
#include "_PDCLIB_io.h"

extern FILE * _PDCLIB_filelist;

// Testing covered by fopen.cpp
FILE * _PDCLIB_fvopen(
    _PDCLIB_fd_t                                    fd,
    const _PDCLIB_fileops_t    *_PDCLIB_restrict    ops,
    int                                             mode,
    const char                  *_PDCLIB_restrict   filename
)
{
    size_t filename_len;
    FILE * rc;
    if ( mode == NULL )
    {
        /* Mode invalid */
        return NULL;
    }
    /* To reduce the number of malloc calls, all data fields are concatenated:
       * the FILE structure itself,
       * ungetc buffer,
       * filename buffer,
       * data buffer.
       Data buffer comes last because it might change in size ( setvbuf() ).
    */
    filename_len = filename ? strlen( filename ) + 1 : 1;
    if ( ( rc = calloc( 1, sizeof( FILE ) + _PDCLIB_UNGETCBUFSIZE + filename_len + BUFSIZ ) ) == NULL )
    {
        /* no memory */
        return NULL;
    }

    if(mtx_init(&rc->lock, mtx_recursive) != thrd_success) {
        free(rc);
        return NULL;
    }

    rc->status = mode;
    rc->ops    = ops;
    rc->handle = fd;
    /* Setting pointers into the memory block allocated above */
    rc->ungetbuf = (unsigned char *)rc + sizeof( FILE );
    rc->filename = (char *)rc->ungetbuf + _PDCLIB_UNGETCBUFSIZE;
    rc->buffer   = rc->filename + filename_len;
    /* Copying filename to FILE structure */
    if(filename) strcpy( rc->filename, filename );
    /* Initializing the rest of the structure */
    rc->bufsize = BUFSIZ;
    rc->bufidx = 0;
#ifdef _PDCLIB_NEED_EOL_TRANSLATION
    rc->bufnlexp = 0;
#endif
    rc->ungetidx = 0;
    /* Setting buffer to _IOLBF because "when opened, a stream is fully
       buffered if and only if it can be determined not to refer to an
       interactive device."
    */
    rc->status |= _IOLBF;
    /* TODO: Setting mbstate */
    /* Adding to list of open files */
    rc->next = _PDCLIB_filelist;
    _PDCLIB_filelist = rc;
    return rc;
}
