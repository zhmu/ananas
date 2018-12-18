/* fwrite( void *, size_t, size_t, FILE * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "_PDCLIB_io.h"

size_t _PDCLIB_fread_unlocked(
    void* _PDCLIB_restrict ptr, size_t size, size_t nmemb, FILE* _PDCLIB_restrict stream)
{
    if (_PDCLIB_prepread(stream) == EOF) {
        return 0;
    }
    char* dest = (char*)ptr;
    size_t nmemb_i;
    for (nmemb_i = 0; nmemb_i < nmemb; ++nmemb_i) {
        size_t numread = _PDCLIB_getchars(&dest[nmemb_i * size], size, EOF, stream);
        if (numread != size)
            break;
    }
    return nmemb_i;
}

size_t
fread_unlocked(void* _PDCLIB_restrict ptr, size_t size, size_t nmemb, FILE* _PDCLIB_restrict stream)
{
    return _PDCLIB_fread_unlocked(ptr, size, nmemb, stream);
}

size_t fread(void* _PDCLIB_restrict ptr, size_t size, size_t nmemb, FILE* _PDCLIB_restrict stream)
{
    _PDCLIB_flockfile(stream);
    size_t r = _PDCLIB_fread_unlocked(ptr, size, nmemb, stream);
    _PDCLIB_funlockfile(stream);
    return r;
}
