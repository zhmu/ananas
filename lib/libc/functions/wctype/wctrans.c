/* wctrans( const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <wctype.h>
#include <string.h>
#include "_PDCLIB_locale.h"

wctrans_t wctrans( const char * property )
{
    if(!property) {
        return 0;
    } else if(strcmp(property, "tolower") == 0) {
        return _PDCLIB_WCTRANS_TOLOWER;
    } else if(strcmp(property, "toupper") == 0) {
        return _PDCLIB_WCTRANS_TOUPPER;
    } else {
        return 0;
    }
}
