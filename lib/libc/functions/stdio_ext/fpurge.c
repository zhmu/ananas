/* __fpurge( FILE * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include <stdio_ext.h>
#include "_PDCLIB_io.h"

void __fpurge( FILE * stream)
{
    _PDCLIB_flockfile( stream );
    stream->bufidx = 0;
#ifdef _PDCLIB_NEED_EOL_TRANSLATION
    stream->bufnlexp = 0;
#endif
    stream->ungetidx = 0;
    _PDCLIB_funlockfile( stream );
}
