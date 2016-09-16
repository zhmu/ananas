/* $Id: strtok.c 416 2010-05-15 00:39:28Z solar $ */

/* strtok( char *, const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>

#ifndef REGTEST

char * strtok_r( char * _PDCLIB_restrict s1, const char * _PDCLIB_restrict s2, char ** _PDCLIB_restrict lasts )
{
    const char * p = s2;

    if ( s1 != NULL )
    {
        /* new string */
        *lasts = s1;
    }
    else
    {
        /* old string continued */
        if ( *lasts == NULL )
        {
            /* No old string, no new string, nothing to do */
            return NULL;
        }
        s1 = *lasts;
    }

    /* skipping leading s2 characters */
    while ( *p && *s1 )
    {
        if ( *s1 == *p )
        {
            /* found seperator; skip and start over */
            ++s1;
            p = s2;
            continue;
        }
        ++p;
    }

    if ( ! *s1 )
    {
        /* no more to parse */
	*lasts = NULL;
        return ( NULL );
    }

    /* skipping non-s2 characters */
    *lasts = s1;
    while ( **lasts )
    {
        p = s2;
        while ( *p )
        {
            if ( **lasts == *p++ )
            {
                /* found seperator; overwrite with '\0', position lasts, return */
                *(*lasts)++ = '\0';
                return s1;
            }
        }
        ++(*lasts);
    }

    /* parsed to end of string */
    *lasts = NULL;
    return s1;
}
#endif

#ifdef TEST
#include <_PDCLIB_test.h>

int main( void )
{
    char* tmp;
    char s[] = "_a_bc__d_";
    TESTCASE( strtok_r( s, "_", &tmp ) == &s[1] );
    TESTCASE( s[1] == 'a' );
    TESTCASE( s[2] == '\0' );
    TESTCASE( strtok_r( NULL, "_", &tmp ) == &s[3] );
    TESTCASE( s[3] == 'b' );
    TESTCASE( s[4] == 'c' );
    TESTCASE( s[5] == '\0' );
    TESTCASE( strtok_r( NULL, "_", &tmp ) == &s[7] );
    TESTCASE( s[6] == '_' );
    TESTCASE( s[7] == 'd' );
    TESTCASE( s[8] == '\0' );
    TESTCASE( strtok_r( NULL, "_", &tmp ) == NULL );
    strcpy( s, "ab_cd" );
    TESTCASE( strtok_r( s, "_", &tmp ) == &s[0] );
    TESTCASE( s[0] == 'a' );
    TESTCASE( s[1] == 'b' );
    TESTCASE( s[2] == '\0' );
    TESTCASE( strtok_r( NULL, "_", &tmp ) == &s[3] );
    TESTCASE( s[3] == 'c' );
    TESTCASE( s[4] == 'd' );
    TESTCASE( s[5] == '\0' );
    TESTCASE( strtok_r( NULL, "_", &tmp ) == NULL );
    return TEST_RESULTS;
}
#endif
