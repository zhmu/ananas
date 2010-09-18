#ifndef __WCHAR_H__
#define __WCHAR_H__

#ifndef _PDCLIB_INT_H
#define _PDCLIB_INT_H _PDCLIB_INT_H
#include <_PDCLIB/_PDCLIB_int.h>
#endif

#include <ananas/types.h>


size_t mbtowc( wchar_t * _PDCLIB_restrict pwc, const char * _PDCLIB_restrict s, size_t n);

#endif /* __WCHAR_H__ */
