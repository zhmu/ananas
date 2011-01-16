#ifndef __I386_STDINT_H__
#define __I386_STDINT_H__

/* 7.18.2.1: Limits of exact-width integer types */
#define INT8_MIN		-127
#define INT16_MIN		-32767
#define INT32_MIN		-2147483648
#define INT64_MIN		-9223372036854775808LL

#define INT8_MAX		127
#define INT16_MAX		32767
#define INT32_MAX		2147483647
#define INT64_MAX		9223372036854775807LL

#define UINT8_MAX		255
#define UINT16_MAX		65535
#define UINT32_MAX		4294967295U
#define UINT64_MAX		18446744073709551615ULL

/* 7.18.2.2: Limits of minimum-width integer types */
#define INT_LEAST8_MIN		INT8_MIN
#define INT_LEAST16_MIN		INT16_MIN
#define INT_LEAST32_MIN		INT32_MIN
#define INT_LEAST64_MIN		INT64_MIN

#define INT_LEAST8_MAX		INT8_MAX
#define INT_LEAST16_MAX		INT16_MAX
#define INT_LEAST32_MAX		INT32_MAX
#define INT_LEAST64_MAX		INT64_MAX

#define UINT_LEAST8_MAX		UINT8_MAX
#define UINT_LEAST16_MAX	UINT16_MAX
#define UINT_LEAST32_MAX	UINT32_MAX
#define UINT_LEAST64_MAX	UINT64_MAX

/* 7.18.2.3: Limits of fastest minimum-width integer types */
#define INT_FAST8_MIN		INT8_MIN
#define INT_FAST16_MIN		INT16_MIN
#define INT_FAST32_MIN		INT32_MIN
#define INT_FAST64_MIN		INT64_MIN

#define INT_FAST8_MAX		INT8_MAX
#define INT_FAST16_MAX		INT16_MAX
#define INT_FAST32_MAX		INT32_MAX
#define INT_FAST64_MAX		INT64_MAX

#define UINT_FAST8_MAX		UINT8_MAX
#define UINT_FAST16_MAX		UINT16_MAX
#define UINT_FAST32_MAX		UINT32_MAX
#define UINT_FAST64_MAX		UINT64_MAX

/* 7.18.2.4: Limits of integer types capable of holding object pointers */
#define INTPTR_MIN		INT64_MIN
#define INTPTR_MAX		INT64_MAX
#define UINTPTR_MAX		UINT64_MAX

/* 7.18.2.5: Limits of greatest-width integer types */
#define INTMAX_MIN		INT64_MIN
#define INTMAX_MAX		INT64_MAX
#define UINTMAX_MAX		UINT64_MAX

/* 7.18.3: Limits of other integer types */
#define PTRDIFF_MIN		INT64_MIN
#define PTRDIFF_MAX		INT64_MAX

#define SIZE_MAX		UINT64_MAX

#define SIG_ATOMIC_MIN		INT32_MIN
#define SIG_ATOMIC_MAX		INT32_MAX

#define WCHAR_MIN		INT32_MIN	/* XXX assume wchar = 32 bit */
#define WCHAR_MAX		INT32_MAX
#define WINT_MIN		INT32_MIN	/* XXX assume wint = 32 bit */
#define WINT_MAX		INT32_MAX

/* 7.18.4: Macros for integer constants */
#define INT8_C(c)		(c)
#define INT16_C(c)		(c)
#define INT32_C(c)		(c)
#define INT64_C(c)		(c ## LL)

#define UINT8_C(c)		(c)
#define UINT16_C(c)		(c)
#define UINT32_C(c)		(c ## U)
#define UINT64_C(c)		(c ## ULL)

#define INTMAX_C(c)		(c ## LL)
#define UINTMAX_C(c)		(c ## ULL)

#endif /* __I386_STDINT_H__ */
