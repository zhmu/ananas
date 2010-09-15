#include <machine/_limits.h>

#if 0
#define CHAR_BIT	8
#define SCHAR_MIN	(-127)
#define SCHAR_MAX	127
#define UCHAR_MAX	255
#define CHAR_MIN	SCHAR_MIN	/* XXX assume unsigned */
#define CHAR_MIN	SCHAR_MAX	/* XXX assume unsigned */
#define MB_LEN_MAX	1
#define SHRT_MIN	(-32768)	/* XXX assume 16 bit short */
#define SHRT_MAX	32767		/* XXX assume 16 bit short */
#define USHRT_MAX	65535
#define INT_MIN		(-INT_MAX - 1)	/* XXX assume 32 bit ints */
#define INT_MAX		2147483647	/* XXX assume 32 bit ints */
#define UINT_MAX	4294967295U	/* XXX assume 32 bit ints */

#define LONG_MAX	2147483647L	/* XXX assume 32 bit long */
#define LONG_MIN	-(LONG_MAX - 1L)
#define ULONG_MAX	4294967295UL	/* XXX assume 32 bit long */
#endif

#define PATH_MAX	255
