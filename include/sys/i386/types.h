#ifndef __I386_TYPES_H__
#define __I386_TYPES_H__

#define PAGE_SIZE	4096

#ifndef ASM

typedef char		int8_t;
typedef unsigned char	uint8_t;
typedef short		int16_t;
typedef unsigned short	uint16_t;
typedef int		int32_t;
typedef unsigned int	uint32_t;
typedef long		int64_t;
typedef unsigned long long uint64_t;

typedef uint32_t	addr_t;
typedef uint32_t	off_t;
typedef uint32_t	size_t;
typedef int32_t		ssize_t;

typedef uint32_t	register_t;

typedef uint32_t	uintptr_t;
typedef uint32_t	ptrdiff_t;

#endif /* ASM */

#endif /* __I386_TYPES_H__ */
