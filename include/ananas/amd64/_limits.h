#ifndef __AMD64_LIMITS_H__
#define __AMD64_LIMITS_H__

/*
 * As per ISO/IEC 9899:1999, 5.2.4.2.
 */
#define CHAR_BIT	8		/* Number of bits in a byte */
#define SCHAR_MIN	-127		/* Minimum value for a signed char */
#define SCHAR_MAX	127		/* Maximum value for a signed char */
#define UCHAR_MAX	255		/* Maximum value for an unsigned char */
#define CHAR_MIN	SCHAR_MIN	/* Minimum value of a char */
#define CHAR_MAX	SCHAR_MAX	/* Maximum value of a char */
#define MB_LEN_MAX	1		/* Maximum number of bytes in a multibyte character */
#define SHRT_MIN	-32767		/* Minimum value for a short int */
#define SHRT_MAX	32767		/* Maximum value for a short int */
#define USHRT_MAX	65535		/* Maximum value for a unsigned short int */
#define INT_MIN		-2147483648	/* Minimum value for an int */
#define INT_MAX		2147483647	/* Maximum value for an int */
#define UINT_MAX	4294967295U	/* Maximum value for an unsigned int */

#define LONG_MAX	9223372036854775807L	/* Maximum value for a long */
#define LONG_MIN	(-LONG_MAX - 1L)	/* Minimum value for a long */
#define ULONG_MAX	18446744073709551615UL	/* Maximum value for a unsigned long */

#define LLONG_MAX	9223372036854775807LL	/* Maximum value for a long long */
#define LLONG_MIN	(-LONG_MAX - 1LL)	/* Minimum value for a long long */
#define ULLONG_MAX	18446744073709551615ULL	/* Maximum value for a unsigned long long */

#endif /* __AMD64_LIMITS_H__ */
