/* strnlen( const char *, size_t len )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>
#include <stdint.h>

size_t strnlen(const char* s, size_t maxlen)
{
    for (size_t len = 0; len != maxlen; len++) {
        if (s[len] == '\0')
            return len;
    }
    return maxlen;
}
