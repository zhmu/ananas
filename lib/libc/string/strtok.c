/* $Id: strtok.c 416 2010-05-15 00:39:28Z solar $ */

/* strtok( char *, const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>

#ifndef REGTEST

char * strtok( char * _PDCLIB_restrict s1, const char * _PDCLIB_restrict s2 )
{
	static char* tmp = NULL;
	return strtok_r(s1, s2, &tmp);
}
#endif

#ifdef TEST
#include <_PDCLIB_test.h>

int main( void )
{
    /* no tests for a simple wrapper */
    return TEST_RESULTS;
}
#endif
