/* $Id: tmpfile.c 366 2009-09-13 15:14:02Z solar $ */

/* tmpfile( void )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>

#ifndef REGTEST

struct _PDCLIB_file_t * tmpfile( void )
{
    /* TODO: Implement */
    return NULL;
}

#endif

#ifdef TEST
#include <_PDCLIB_test.h>

int main()
{
    TESTCASE( NO_TESTDRIVER );
    return TEST_RESULTS;
}

#endif
