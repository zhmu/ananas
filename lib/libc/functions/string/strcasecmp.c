/* strcasecmp( const char *, const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <strings.h>
#include <ctype.h>

#ifndef REGTEST

int strcasecmp( const char * s1, const char * s2 )
{
    while ( ( *s1 ) && ( tolower(*s1) == tolower(*s2) ) )
    {
        ++s1;
        ++s2;
    }
    return ( tolower(*(unsigned char *)s1) - tolower(*(unsigned char *)s2) );
}

#endif

#ifdef TEST
#include "_PDCLIB_test.h"

int main( void )
{
    char cmpabcde[] = "abcde";
    char cmpaBCd_[] = "aBCd\xfc";
    char empty[] = "";
    TESTCASE( strcasecmp( abcde, cmpabcde ) == 0 );
    TESTCASE( strcasecmp( abcde, abcdx ) < 0 );
    TESTCASE( strcasecmp( abcdx, abcde ) > 0 );
    TESTCASE( strcasecmp( empty, abcde ) < 0 );
    TESTCASE( strcasecmp( abcde, empty ) > 0 );
    TESTCASE( strcasecmp( abcde, cmpabcd_ ) < 0 );
    return TEST_RESULTS;
}
#endif
