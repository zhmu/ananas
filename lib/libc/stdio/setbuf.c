/* $Id: setbuf.c 366 2009-09-13 15:14:02Z solar $ */

/* setbuf( FILE *, char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>

#ifndef REGTEST

void setbuf( struct _PDCLIB_file_t * _PDCLIB_restrict stream, char * _PDCLIB_restrict buf )
{
    if ( buf == NULL )
    {
        setvbuf( stream, buf, _IONBF, BUFSIZ );
    }
    else
    {
        setvbuf( stream, buf, _IOFBF, BUFSIZ );
    }
}

#endif

#ifdef TEST
#include <_PDCLIB_test.h>
#include <stdlib.h>

int main( void )
{
    /* TODO: Extend testing once setvbuf() is finished. */
#ifndef REGTEST
    char const * const filename = "testfile";
    char buffer[ BUFSIZ + 1 ];
    FILE * fh;
    remove( filename );
    /* full buffered */
    TESTCASE( ( fh = fopen( filename, "w" ) ) != NULL );
    setbuf( fh, buffer );
    TESTCASE( fh->buffer == buffer );
    TESTCASE( fh->bufsize == BUFSIZ );
    TESTCASE( ( fh->status & ( _IOFBF | _IONBF | _IOLBF ) ) == _IOFBF );
    TESTCASE( fclose( fh ) == 0 );
    TESTCASE( remove( filename ) == 0 );
    /* not buffered */
    TESTCASE( ( fh = fopen( filename, "w" ) ) != NULL );
    setbuf( fh, NULL );
    TESTCASE( ( fh->status & ( _IOFBF | _IONBF | _IOLBF ) ) == _IONBF );
    TESTCASE( fclose( fh ) == 0 );
    TESTCASE( remove( filename ) == 0 );
#else
    puts( " NOTEST setbuf() test driver is PDCLib-specific." );
#endif
    return TEST_RESULTS;
}

#endif