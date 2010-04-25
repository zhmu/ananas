/* $Id: abort.c 152 2006-03-08 15:36:14Z solar $ */

/* abort( void )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdlib.h>

#ifndef REGTEST

void abort( void )
{
    /* XXX ananas: we do not support signals */
    *(int*)NULL = 0;
#if 0
    raise( SIGABRT );
#endif
    exit( EXIT_FAILURE );
}

#endif

#ifdef TEST
#include <_PDCLIB_test.h>

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
