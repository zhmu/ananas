/* exit( int )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdlib.h>

extern void __cxa_finalize(void*);

void exit( int status )
{
    __cxa_finalize(NULL);
    _Exit( status );
}
