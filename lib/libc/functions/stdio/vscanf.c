/* vscanf( const char *, va_list )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include <stdarg.h>
#include "_PDCLIB_io.h"

int _PDCLIB_vscanf_unlocked( const char * _PDCLIB_restrict format,
                             _PDCLIB_va_list arg )
{
    return _PDCLIB_vfscanf_unlocked( stdin, format, arg );
}

int vscanf_unlocked( const char * _PDCLIB_restrict format,
                     _PDCLIB_va_list arg )
{
    return _PDCLIB_vscanf_unlocked(format, arg);
}

// Testing covered by scanf.cpp
int vscanf( const char * _PDCLIB_restrict format, _PDCLIB_va_list arg )
{
    return vfscanf( stdin, format, arg );
}
