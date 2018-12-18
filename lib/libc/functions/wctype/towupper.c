/* towupper( wint_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <wctype.h>
#include "_PDCLIB_locale.h"

wint_t _PDCLIB_towupper_l(wint_t wc, locale_t l)
{
    wint_t uwc = _PDCLIB_unpackwint(wc);
    _PDCLIB_wcinfo_t* info = _PDCLIB_wcgetinfo(l, uwc);
    if (info) {
        uwc += info->upper_delta;
    }
    return uwc;
}

wint_t towupper(wint_t wc) { return _PDCLIB_towupper_l(wc, _PDCLIB_threadlocale()); }
