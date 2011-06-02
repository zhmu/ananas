/* $Id: stddef.h 416 2010-05-15 00:39:28Z solar $ */

/* 7.17 Common definitions <stddef.h>

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#ifndef _PDCLIB_STDDEF_H
#define _PDCLIB_STDDEF_H _PDCLIB_STDDEF_H

/* Import size_t and ptrdiff_t, which are architecture-dependant */
#include <machine/_stddef.h>

/* NULL */
#include <ananas/_types/null.h>

#ifndef _PDCLIB_CONFIG_H
#define _PDCLIB_CONFIG_H _PDCLIB_CONFIG_H
#include <_PDCLIB/_PDCLIB_config.h>
#endif

typedef int wchar_t;

#ifndef _PDCLIB_INT_H
#define _PDCLIB_INT_H _PDCLIB_INT_H
#include <_PDCLIB/_PDCLIB_int.h>
#endif

#define offsetof( type, member ) _PDCLIB_offsetof( type, member )

#endif
