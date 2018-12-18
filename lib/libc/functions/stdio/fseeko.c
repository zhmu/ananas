/* fseek( FILE *, long, int )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include "_PDCLIB_io.h"

int _PDCLIB_fseeko_unlocked(FILE* stream, off_t offset, int whence)
{
    if (stream->status & _PDCLIB_FWRITE) {
        if (_PDCLIB_flushbuffer(stream) == EOF) {
            return EOF;
        }
    }
    stream->status &= ~_PDCLIB_EOFFLAG;
    if (stream->status & _PDCLIB_FRW) {
        stream->status &= ~(_PDCLIB_FREAD | _PDCLIB_FWRITE);
    }

    if (whence == SEEK_CUR) {
        whence = SEEK_SET;
        offset += _PDCLIB_ftell64_unlocked(stream);
    }

    return (_PDCLIB_seek(stream, offset, whence) != EOF) ? 0 : EOF;
}

int fseeko_unlocked(FILE* stream, off_t loffset, int whence)
{
    return _PDCLIB_fseeko_unlocked(stream, loffset, whence);
}

int fseeko(FILE* stream, off_t loffset, int whence)
{
    _PDCLIB_flockfile(stream);
    int r = _PDCLIB_fseeko_unlocked(stream, loffset, whence);
    _PDCLIB_funlockfile(stream);
    return r;
}
