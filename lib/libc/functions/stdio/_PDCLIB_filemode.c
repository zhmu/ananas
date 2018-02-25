/* _PDCLIB_filemode( const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stddef.h>
#include "_PDCLIB_io.h"

/* Helper function that parses the C-style mode string passed to fopen() into
   the PDCLib flags FREAD, FWRITE, FAPPEND, FRW (read-write) and FBIN (binary
   mode).
*/
unsigned int _PDCLIB_filemode( char const * const mode )
{
    if(!mode) return 0;

    unsigned rc = 0;
    switch ( mode[0] )
    {
        case 'r':
            rc |= _PDCLIB_FREAD;
            break;
        case 'w':
            rc |= _PDCLIB_FWRITE;
            break;
        case 'a':
            rc |= _PDCLIB_FAPPEND | _PDCLIB_FWRITE;
            break;
        default:
            /* Other than read, write, or append - invalid */
            return 0;
    }
    for ( size_t i = 1; i < 4; ++i )
    {
        switch ( mode[i] )
        {
            case '+':
                if ( rc & _PDCLIB_FRW ) return 0; /* Duplicates are invalid */
                rc |= _PDCLIB_FRW;
                break;
            case 'b':
                if ( rc & _PDCLIB_FBIN ) return 0; /* Duplicates are invalid */
                rc |= _PDCLIB_FBIN;
                break;
            case '\0':
                /* End of mode */
                return rc;
            default:
                /* Other than read/write or binary - invalid. */
                return 0;
        }
    }
    /* Longer than three chars - invalid. */
    return 0;
}
