/* float strtof( const char * nptr, char * * endptr )


   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdlib.h>

long double strtold(const char* nptr, char** endptr)
{
    /* XXXRS This may be a bit too short-circuited... */
    return (long double)strtod(nptr, endptr);
}
