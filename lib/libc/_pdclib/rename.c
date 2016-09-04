/* $Id: rename.c 366 2009-09-13 15:14:02Z solar $ */

/* _PDCLIB_rename( const char *, const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include <errno.h>

#ifndef REGTEST
#include <_PDCLIB/_PDCLIB_glue.h>

#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <string.h>

int _PDCLIB_rename( const char * old, const char * new )
{
    errorcode_t err = sys_rename(old, new);
    if (err == ANANAS_ERROR_NONE)
	return 0;
   
    _posix_map_error(err); 
    return EOF;
}

#endif

#ifdef TEST
#include <_PDCLIB/_PDCLIB_test.h>

#include <stdlib.h>

int main( void )
{
    char filename1[] = "touch testfile1";
    char filename2[] = "testfile2";
    remove( filename1 + 6 );
    remove( filename2 );
    /* check that neither file exists */
    TESTCASE( fopen( filename1 + 6, "r" ) == NULL );
    TESTCASE( fopen( filename2, "r" ) == NULL );
    /* rename file 1 to file 2 - expected to fail */
    TESTCASE( _PDCLIB_rename( filename1 + 6, filename2 ) == -1 );
    /* create file 1 */
    system( filename1 );
    /* check that file 1 exists */
    TESTCASE( fopen( filename1 + 6, "r" ) != NULL );
    /* rename file 1 to file 2 */
    TESTCASE( _PDCLIB_rename( filename1 + 6, filename2 ) == 0 );
    /* check that file 2 exists, file 1 does not */
    TESTCASE( fopen( filename1 + 6, "r" ) == NULL );
    TESTCASE( fopen( filename2, "r" ) != NULL );
    /* create another file 1 */
    system( filename1 );
    /* check that file 1 exists */
    TESTCASE( fopen( filename1 + 6, "r" ) != NULL );
    /* rename file 1 to file 2 - expected to fail, see comment in
       _PDCLIB_rename() itself.
    */
    TESTCASE( _PDCLIB_rename( filename1 + 6, filename2 ) == -1 );
    /* remove both files */
    remove( filename1 + 6 );
    remove( filename2 );
    /* check that they're gone */
    TESTCASE( fopen( filename1 + 6, "r" ) == NULL );
    TESTCASE( fopen( filename2, "r" ) == NULL );
    return TEST_RESULTS;
}

#endif
