/* [XSI] char * strdup( const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <stdlib.h>

char *strdup(const char *s)
{
    char* ns = NULL;
    if(s) {
        size_t len = strlen(s) + 1;
        ns = malloc(len);
        if(ns)
            strncpy(ns, s, len);
    }
    return ns;
}
