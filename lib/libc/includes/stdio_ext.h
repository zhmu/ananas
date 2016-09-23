/* Input/output extensions <stdio_ext.h>

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#ifndef _PDCLIB_STDIO_EXT_H
#define _PDCLIB_STDIO_EXT_H _PDCLIB_STDIO_EXT_H
#include "_PDCLIB_int.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The following are platform-dependant, and defined in _PDCLIB_config.h. */
typedef _PDCLIB_file_t FILE;

void __fpurge(FILE*);

#ifdef __cplusplus
}
#endif

#endif
