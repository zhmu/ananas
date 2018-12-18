/* Errors <errno.h>

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#ifndef _PDCLIB_ERRNO_H
#define _PDCLIB_ERRNO_H _PDCLIB_ERRNO_H

#include <ananas/errno.h>
#include "_PDCLIB_int.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int* _PDCLIB_errno_func(void);
#define errno (*_PDCLIB_errno_func())

#ifdef __cplusplus
}
#endif

#endif
