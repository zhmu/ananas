/* fileno( FILE* f )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>

#ifndef REGTEST
#include "_PDCLIB_glue.h"
#include "_PDCLIB_io.h"
#include <errno.h>

int fileno(FILE* stream)
{
    if (stream == NULL) {
        errno = EBADF;
        return -1;
    }

    return stream->handle.sval;
}

#endif
