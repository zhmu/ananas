/* ftrylockfile( FILE * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include <stdarg.h>
#include <threads.h>
#include <stdlib.h>
#include "_PDCLIB_io.h"

int _PDCLIB_ftrylockfile(FILE* file)
{
    int res = mtx_trylock(&file->lock);
    switch (res) {
        case thrd_success:
            return 0;
        case thrd_busy:
            return 1;

        default:
            abort();
    }
}
