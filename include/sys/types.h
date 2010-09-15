#ifndef __TYPES_H__
#define __TYPES_H__

#ifndef ASM

#include <machine/_types.h>
#include <sys/_types/null.h>
#include <sys/_types/pid.h>
#include <sys/_types/uid.h>
#include <sys/_types/gid.h>
#include <sys/_types/mode.h>
#include <sys/_types/ino.h>
#include <sys/_types/nlink.h>
#include <sys/_types/dev.h>
#include <sys/_types/blksize.h>
#include <sys/_types/blkcnt.h>
#include <sys/_types/block.h>
#include <sys/_types/time.h>

typedef unsigned char	u_char;
typedef unsigned short	u_short;
typedef unsigned int	u_int;
typedef unsigned long	u_long;
typedef char*		caddr_t;

typedef	uint32_t	suseconds_t;

#define __STRING(x) #x
#define STRINGIFY(x) __STRING(x)

#endif /* !ASM */

#endif /* __TYPES_H__ */
