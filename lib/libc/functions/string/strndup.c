/* [XSI] char * strndup( const char *, size_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <stdlib.h>

char *strndup( const char * s, size_t len )
{
    char* ns = NULL;
    if(s) {
        ns = malloc(len + 1);
        if(ns) {
            ns[len] = 0;
            // strncpy to be pedantic about modification in multithreaded
            // applications
            return strncpy(ns, s, len);
        }
    }
    return ns;
}
