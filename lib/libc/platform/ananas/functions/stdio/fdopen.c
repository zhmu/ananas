/* fdopen( int fildes, const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>

#ifndef REGTEST
#include "_PDCLIB_io.h"
#include "_PDCLIB_glue.h"

extern const _PDCLIB_fileops_t _PDCLIB_fileops;

FILE* fdopen( int fildes, const char * _PDCLIB_restrict mode)
{
    int imode = _PDCLIB_filemode( mode );
    
    if( imode == 0 )
        return NULL;

    _PDCLIB_fd_t fd;
    fd.sval = fildes;
    return _PDCLIB_fvopen( fd, &_PDCLIB_fileops, imode, NULL );
}

#endif
