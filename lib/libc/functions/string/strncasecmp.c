/* strncasecmp( const char *, const char *, size_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>
#include <ctype.h>

#ifndef REGTEST

int strncasecmp( const char * s1, const char * s2, size_t n )
{
    while ( *s1 && n && ( tolower(*s1) == tolower(*s2) ) )
    {
        ++s1;
        ++s2;
        --n;
    }
    if ( n == 0 )
    {
        return 0;
    }
    else
    {
        return ( tolower(*(unsigned char *)s1) - tolower(*(unsigned char *)s2) );
    }
}

#endif

#ifdef TEST
#include "_PDCLIB_test.h"

int main( void )
{
    char cmpabcde[] = "abcde\0f";
    char cmpaBCd_[] = "aBCde\xfc";
    char empty[] = "";
    char x[] = "x";
    TESTCASE( strncasecmp( abcde, cmpabcde, 5 ) == 0 );
    TESTCASE( strncasecmp( abcde, cmpabcde, 10 ) == 0 );
    TESTCASE( strncasecmp( abcde, abcdx, 5 ) < 0 );
    TESTCASE( strncasecmp( abcdx, abcde, 5 ) > 0 );
    TESTCASE( strncasecmp( empty, abcde, 5 ) < 0 );
    TESTCASE( strncasecmp( abcde, empty, 5 ) > 0 );
    TESTCASE( strncasecmp( abcde, abcdx, 4 ) == 0 );
    TESTCASE( strncasecmp( abcde, x, 0 ) == 0 );
    TESTCASE( strncasecmp( abcde, x, 1 ) < 0 );
    TESTCASE( strncasecmp( abcde, cmpaBCd_, 10 ) < 0 );
    return TEST_RESULTS;
}
#endif
