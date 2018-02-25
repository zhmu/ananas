/* towctrans( wint_t, wctrans_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <wctype.h>
#include <string.h>
#include "_PDCLIB_locale.h"

wint_t _PDCLIB_towctrans_l( wint_t wc, wctrans_t trans, locale_t l )
{
    switch( trans ) {
        case 0:                         return wc;
        case _PDCLIB_WCTRANS_TOLOWER:   return _PDCLIB_towlower_l( wc, l );
        case _PDCLIB_WCTRANS_TOUPPER:   return _PDCLIB_towupper_l( wc, l );
        default: abort();
    }
}

wint_t towctrans( wint_t wc, wctrans_t trans )
{
    return _PDCLIB_towctrans_l( wc, trans, _PDCLIB_threadlocale() );
}
