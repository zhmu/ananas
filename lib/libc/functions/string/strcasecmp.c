/* strcasecmp( const char *, const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <strings.h>
#include <ctype.h>

int strcasecmp(const char* s1, const char* s2)
{
    while ((*s1) && (tolower(*s1) == tolower(*s2))) {
        ++s1;
        ++s2;
    }
    return (tolower(*(unsigned char*)s1) - tolower(*(unsigned char*)s2));
}
