/* getc( FILE * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include "_PDCLIB_io.h"

int _PDCLIB_getc_unlocked(FILE* stream) { return _PDCLIB_fgetc_unlocked(stream); }

int getc_unlocked(FILE* stream) { return _PDCLIB_getc_unlocked(stream); }

// Testing covered by ftell.cpp
int getc(FILE* stream) { return fgetc(stream); }
