/* atof( const char* nptr )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdlib.h>

double atof(const char* nptr) { return strtod(nptr, NULL); }
