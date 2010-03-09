#ifndef __TYPES_H__
#define __TYPES_H__

#ifndef ASM

#include <machine/_types.h>
#include <_null.h>

typedef unsigned char	u_char;
typedef unsigned short	u_short;
typedef unsigned int	u_int;
typedef unsigned long	u_long;
typedef char*		caddr_t;

#endif

#define __STRING(x) #x
#define STRINGIFY(x) __STRING(x)

#endif /* __TYPES_H__ */
