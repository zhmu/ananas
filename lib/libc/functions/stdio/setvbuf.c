/* setvbuf( FILE *, char *, int, size_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "_PDCLIB_io.h"

int setvbuf(FILE* _PDCLIB_restrict stream, char* _PDCLIB_restrict buf, int mode, size_t size)
{
    _PDCLIB_flockfile(stream);
    switch (mode) {
        case _IONBF:
            /* When unbuffered I/O is requested, we keep the buffer anyway, as
               we don't want to e.g. flush the stream for every character of a
               stream being printed.
            */
            break;
        case _IOFBF:
        case _IOLBF:
            if (size > INT_MAX || size == 0) {
                /* PDCLib only supports buffers up to INT_MAX in size. A size
                   of zero doesn't make sense.
                */
                _PDCLIB_funlockfile(stream);
                return -1;
            }
            if (buf == NULL) {
                /* User requested buffer size, but leaves it to library to
                   allocate the buffer.
                */
                /* If current buffer is big enough for requested size, but not
                   over twice as big (and wasting memory space), we use the
                   current buffer (i.e., do nothing), to save the malloc() /
                   free() overhead.
                */
                if ((stream->bufsize < size) || (stream->bufsize > (size << 1))) {
                    /* Buffer too small, or much too large - allocate. */
                    if ((buf = (char*)malloc(size)) == NULL) {
                        /* Out of memory error. */
                        _PDCLIB_funlockfile(stream);
                        return -1;
                    }
                    /* This buffer must be free()d on fclose() */
                    stream->status |= _PDCLIB_FREEBUFFER;
                }
            }
            stream->buffer = buf;
            stream->bufsize = size;
            break;
        default:
            /* If mode is something else than _IOFBF, _IOLBF or _IONBF -> exit */
            _PDCLIB_funlockfile(stream);
            return -1;
    }
    /* Deleting current buffer mode */
    stream->status &= ~(_IOFBF | _IOLBF | _IONBF);
    /* Set user-defined mode */
    stream->status |= mode;
    _PDCLIB_funlockfile(stream);
    return 0;
}
