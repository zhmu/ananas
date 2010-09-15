/* $Id: close.c 366 2009-09-13 15:14:02Z solar $ */

/* _PDCLIB_close( _PDCLIB_fd_t fd )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

/* This is an example implementation of _PDCLIB_close() fit for use with POSIX
   kernels.
*/

#include <unistd.h>

#ifndef REGTEST
#include <_PDCLIB/_PDCLIB_glue.h>

int _PDCLIB_close( _PDCLIB_fd_t fd )
{
    return close( fd );
}

#endif

#ifdef TEST
#include <_PDCLIB_test.h>

int main( void )
{
    /* No testdriver; tested in driver for _PDCLIB_open(). */
    return TEST_RESULTS;
}

#endif
