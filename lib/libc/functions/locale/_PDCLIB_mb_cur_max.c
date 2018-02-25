/* _PDCLIB_mb_cur_max( void ) 

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <locale.h>
#include "_PDCLIB_locale.h"
#include "_PDCLIB_encoding.h"

size_t _PDCLIB_mb_cur_max( void )
{
    return _PDCLIB_threadlocale()->_Codec->__mb_max;
}
