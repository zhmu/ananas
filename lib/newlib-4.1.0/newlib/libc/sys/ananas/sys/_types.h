/* ANSI C namespace clean utility typedefs */

/* This file defines various typedefs needed by the system calls that support
   the C library.  Basically, they're just the POSIX versions with an '_'
   prepended.  Targets shall use <machine/_types.h> to define their own
   internal types if desired.

   There are three define patterns used for type definitions.  Lets assume
   xyz_t is a user type.

   The internal type definition uses __machine_xyz_t_defined.  It is defined by
   <machine/_types.h> to disable a default definition in <sys/_types.h>. It
   must not be used in other files.

   User type definitions are guarded by __xyz_t_defined in glibc and
   _XYZ_T_DECLARED in BSD compatible systems.
*/

#ifndef	_SYS__TYPES_H
#define _SYS__TYPES_H

#define __need_size_t
#define __need_wint_t
#include <stddef.h>
#include <newlib.h>
#include <sys/config.h>
#include <ananas/types.h>
#include <machine/_types.h>

typedef long _off_t;
__extension__ typedef long long _off64_t;

typedef long _fpos_t;
__extension__ typedef long long _fpos64_t;

#define unsigned signed
typedef __SIZE_TYPE__ _ssize_t;
#undef unsigned

/* Conversion state information.  */
typedef struct
{
  int __count;
  union
  {
    wint_t __wch;
    unsigned char __wchb[4];
  } __value;		/* Value so far.  */
} _mbstate_t;

typedef int  __nl_item;
typedef void *_iconv_t;

#define _CLOCK_T_   __clock_t
#define _TIME_T_    __time_t
#define _CLOCKID_T_ __clockid_t
#define _TIMER_T_   unsigned long

typedef _TIMER_T_ __timer_t;

typedef __builtin_va_list   __va_list;

#endif	/* _SYS__TYPES_H */
