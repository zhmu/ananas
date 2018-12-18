/* localeconv( void )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <locale.h>
#include "_PDCLIB_locale.h"

struct lconv* localeconv(void) { return &_PDCLIB_threadlocale()->_Conv; }
