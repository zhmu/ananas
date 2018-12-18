/* mbsinit( mbstate_t * ps )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <wchar.h>
#include "_PDCLIB_encoding.h"
#include "_PDCLIB_locale.h"

static int _PDCLIB_mbsinit_l(const mbstate_t* ps, locale_t l)
{
    if (ps) {
        return ps->_Surrogate == 0 && ps->_PendState == 0 && l->_Codec->__mbsinit(ps);
    } else
        return 1;
}

int mbsinit(const mbstate_t* ps) { return _PDCLIB_mbsinit_l(ps, _PDCLIB_threadlocale()); }
