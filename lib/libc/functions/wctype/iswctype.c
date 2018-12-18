/* iswctype( wint_t, wctype_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <wctype.h>
#include "_PDCLIB_locale.h"

int _PDCLIB_iswctype_l(wint_t wc, wctype_t desc, locale_t l)
{
    wc = _PDCLIB_unpackwint(wc);

    _PDCLIB_wcinfo_t* info = _PDCLIB_wcgetinfo(l, wc);

    if (!info)
        return 0;

    return info->flags & desc;
}

int iswctype(wint_t wc, wctype_t desc)
{
    return _PDCLIB_iswctype_l(wc, desc, _PDCLIB_threadlocale());
}
