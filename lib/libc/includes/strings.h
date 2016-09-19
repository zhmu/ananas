/* String operations <strings.h>

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#ifndef _PDCLIB_STRINGS_H
#define _PDCLIB_STRINGS_H _PDCLIB_STRINGS_H
#include "_PDCLIB_int.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _PDCLIB_SIZE_T_DEFINED
#define _PDCLIB_SIZE_T_DEFINED _PDCLIB_SIZE_T_DEFINED
typedef _PDCLIB_size_t size_t;
#endif

int strcasecmp(const char*, const char*) _PDCLIB_nothrow;
int strncasecmo(const char*, const char*, size_t) _PDCLIB_nothrow;

#ifdef __cplusplus
}
#endif

#endif
