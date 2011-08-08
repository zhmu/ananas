#ifndef __ANANAS_TYPES_H__
#define __ANANAS_TYPES_H__

#ifndef ASM

#include <machine/_types.h>
#include <machine/_stddef.h>
#include <ananas/_types/null.h>
#include <ananas/_types/pid.h>
#include <ananas/_types/uid.h>
#include <ananas/_types/gid.h>
#include <ananas/_types/mode.h>
#include <ananas/_types/handle.h>
#include <ananas/_types/ino.h>
#include <ananas/_types/nlink.h>
#include <ananas/_types/dev.h>
#include <ananas/_types/blksize.h>
#include <ananas/_types/blkcnt.h>
#include <ananas/_types/blocknr.h>
#include <ananas/_types/errorcode.h>
#include <ananas/_types/refcount.h>
#include <ananas/_types/time.h>
#include <ananas/_types/thread.h>


typedef unsigned char	u_char;
typedef unsigned short	u_short;
typedef unsigned int	u_int;
typedef unsigned long	u_long;
typedef char*		caddr_t;

typedef	uint32_t	suseconds_t;

/* STRINGIFY(x) creates a string of whatever 'x' is => "x" */
#define __STRING(x) #x
#define STRINGIFY(x) __STRING(x)

/* CONCAT(x,y) concatenates identifiers 'x' and 'y' => xy */
#define __CONCAT(x,y) x ## y
#define CONCAT(x,y) __CONCAT(x,y)

#endif /* !ASM */

#endif /* __ANANAS_TYPES_H__ */
