#ifndef __AMD64_TYPES_H__
#define __AMD64_TYPES_H__

#define PAGE_SIZE	4096

#ifndef ASM

typedef char		int8_t;
typedef unsigned char	uint8_t;
typedef short		int16_t;
typedef unsigned short	uint16_t;
typedef int		int32_t;
typedef unsigned int	uint32_t;
typedef long		int64_t;
typedef unsigned long	uint64_t;

typedef uint64_t	addr_t;
typedef uint64_t	off_t;
typedef uint64_t	size_t;
typedef int64_t		ssize_t;

typedef unsigned long	register_t;

typedef uint64_t	uintptr_t;
typedef uint64_t	ptrdiff_t;

#endif /* ASM */

#endif /* __AMD64_TYPES_H__ */
