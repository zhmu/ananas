/* strncasecmp( const char *, const char *, size_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>
#include <ctype.h>

int strncasecmp(const char* s1, const char* s2, size_t n)
{
    while (*s1 && n && (tolower(*s1) == tolower(*s2))) {
        ++s1;
        ++s2;
        --n;
    }
    if (n == 0) {
        return 0;
    } else {
        return (tolower(*(unsigned char*)s1) - tolower(*(unsigned char*)s2));
    }
}
