/* abort( void )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdlib.h>
#include <signal.h>

void abort( void )
{
    raise( SIGABRT );
    exit( EXIT_FAILURE );
}

#ifdef TEST_TODO // XXX How to test this in gtest?
#include "_PDCLIB_test.h"

#include <stdio.h>

static void aborthandler( int sig )
{
    exit( 0 );
}

int main( void )
{
    int UNEXPECTED_RETURN_FROM_ABORT = 0;
    TESTCASE( signal( SIGABRT, &aborthandler ) != SIG_ERR );
    abort();
    TESTCASE( UNEXPECTED_RETURN_FROM_ABORT );
    return TEST_RESULTS;
}

#endif
