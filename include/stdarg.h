#include "machine/types.h"

#ifndef __VARARGS_H__
#define __VARARGS_H__

/*
 * Just chain to whatever the compiler uses.
 */
typedef __builtin_va_list va_list;
#define va_start __builtin_va_start
#define va_arg __builtin_va_arg
#define va_end __builtin_va_end


#endif /* __VARARGS_H__ */
