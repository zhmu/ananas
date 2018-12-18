/* ftell( FILE * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include "_PDCLIB_io.h"

off_t _PDCLIB_ftello_unlocked(FILE* stream)
{
    off_t off64 = _PDCLIB_ftell64_unlocked(stream);

    if (off64 > LONG_MAX) {
        /* integer overflow */
        errno = ERANGE;
        return -1;
    }
    return off64;
}

off_t ftello_unlocked(FILE* stream) { return _PDCLIB_ftello_unlocked(stream); }

off_t ftello(FILE* stream)
{
    _PDCLIB_flockfile(stream);
    off_t off = _PDCLIB_ftello_unlocked(stream);
    _PDCLIB_funlockfile(stream);
    return off;
}
