/* $Id: fopen.c 366 2009-09-13 15:14:02Z solar $ */

/* fopen( const char *, const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include <stdlib.h>

#ifndef REGTEST
#include <_PDCLIB/_PDCLIB_glue.h>

extern struct _PDCLIB_file_t * _PDCLIB_filelist;

struct _PDCLIB_file_t * fopen( const char * _PDCLIB_restrict filename, const char * _PDCLIB_restrict mode )
{
    if ( mode == NULL || filename == NULL || filename[0] == '\0' )
    {
        /* Mode or filename invalid */
        return NULL;
    }

    int fd = _PDCLIB_open( filename, _PDCLIB_filemode(mode) );
    if ( fd == _PDCLIB_NOHANDLE )
    {
        /* OS open() failed */
        return NULL;
    }

    struct _PDCLIB_file_t* rc = fdopen( fd, mode );
    if (rc == NULL) {
        /* open failure */
        _PDCLIB_close( fd );
    }
    return rc;
}

#endif

#ifdef TEST
#include <_PDCLIB_test.h>

int main( void )
{
    /* Some of the tests are not executed for regression tests, as the libc on
       my system is at once less forgiving (segfaults on mode NULL) and more
       forgiving (accepts undefined modes).
    */
    remove( "testfile" );
    TESTCASE_NOREG( fopen( NULL, NULL ) == NULL );
    TESTCASE( fopen( NULL, "w" ) == NULL );
    TESTCASE_NOREG( fopen( "", NULL ) == NULL );
    TESTCASE( fopen( "", "w" ) == NULL );
    TESTCASE( fopen( "foo", "" ) == NULL );
    TESTCASE_NOREG( fopen( "testfile", "wq" ) == NULL ); /* Undefined mode */
    TESTCASE_NOREG( fopen( "testfile", "wr" ) == NULL ); /* Undefined mode */
    TESTCASE( fopen( "testfile", "w" ) != NULL );
    remove( "testfile" );
    return TEST_RESULTS;
}

#endif
