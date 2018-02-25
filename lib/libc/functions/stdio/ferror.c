/* ferror( FILE * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include "_PDCLIB_io.h"

int _PDCLIB_ferror_unlocked( FILE * stream )
{
    return stream->status & _PDCLIB_ERRORFLAG;
}

int ferror_unlocked( FILE * stream )
{
    return _PDCLIB_ferror_unlocked( stream );
}

// Testing covered by clearerr.cpp
int ferror( FILE * stream )
{
    _PDCLIB_flockfile( stream );
    int error = _PDCLIB_ferror_unlocked( stream );
    _PDCLIB_funlockfile( stream );
    return error;
}
