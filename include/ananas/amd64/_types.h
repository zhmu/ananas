/*
 * This file is used to define integer types as per ISO/IEC 9899.
 */
#include <stddef.h>
#ifndef __AMD64_TYPES_H__
#define __AMD64_TYPES_H__

/* 7.18.1.1: Exact-width integer types */
typedef char		int8_t;
typedef unsigned char	uint8_t;
typedef short		int16_t;
typedef unsigned short	uint16_t;
typedef int		int32_t;
typedef unsigned int	uint32_t;
typedef long		int64_t;
typedef unsigned long	uint64_t;

/* 7.18.1.2: Minimum-width integer types */
typedef uint8_t		uint_least8_t;
typedef uint16_t	uint_least16_t;
typedef uint32_t	uint_least32_t;
typedef uint64_t	uint_least64_t;
typedef int8_t		int_least8_t;
typedef int16_t		int_least16_t;
typedef int32_t		int_least32_t;
typedef int64_t		int_least64_t;

/* 7.18.1.3: Fastest minimum-width integer types */
typedef uint8_t		uint_fast8_t;
typedef uint16_t	uint_fast16_t;
typedef uint32_t	uint_fast32_t;
typedef uint64_t	uint_fast64_t;
typedef int8_t		int_fast8_t;
typedef int16_t		int_fast16_t;
typedef int32_t		int_fast32_t;
typedef int64_t		int_fast64_t;

/* 7.18.1.4: Integer types capable of holding pointers */
typedef int64_t		intptr_t;
typedef uint64_t	uintptr_t;

/* 7.18.1.5: Greatest-width integer types */
typedef int64_t		intmax_t;
typedef uint64_t	uintmax_t;

#if 0
/* 7.17.2: Common definitions for stddef.h */
typedef uint64_t	ptrdiff_t;
typedef uint64_t	size_t;
#endif

/*
 * Below are standard types.
 */
typedef uint64_t	addr_t;
typedef int64_t		off_t;
typedef int64_t		ssize_t;
typedef uint64_t	register_t;
typedef int32_t		clock_t;

/* XXX does not belong here */
#define	CLOCKS_PER_SEC	1

#endif /* __AMD64_TYPES_H__ */
