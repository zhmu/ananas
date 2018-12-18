/* _PDCLIB_errno

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <errno.h>
#include <threads.h>

/* Temporary */

static int _PDCLIB_errno = 0;

int* _PDCLIB_errno_func() { return &_PDCLIB_errno; }
