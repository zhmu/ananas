/* iswgraph( wint_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <wctype.h>
#include "_PDCLIB_locale.h"

int iswgraph( wint_t wc )
{
    return iswctype( wc, _PDCLIB_CTYPE_GRAPH );
}
