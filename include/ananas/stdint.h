#ifndef __ANANAS_STDINT_H__
#define __ANANAS_STDINT_H__

#include <machine/_types.h>
#include <machine/_stdint.h>

#ifndef UINT8_T_DECLARED
#define UINT8_T_DECLARED
typedef __uint8_t uint8_t;
#endif

#ifndef UINT16_T_DECLARED
#define UINT16_T_DECLARED
typedef __uint16_t uint16_t;
#endif

#ifndef UINT32_T_DECLARED
#define UINT32_T_DECLARED
typedef __uint32_t uint32_t;
#endif

#ifndef UINT64_T_DECLARED
#define UINT64_T_DECLARED
typedef __uint64_t uint64_t;
#endif

#ifndef INT8_T_DECLARED
#define INT8_T_DECLARED
typedef __int8_t int8_t;
#endif

#ifndef INT16_T_DECLARED
#define INT16_T_DECLARED
typedef __int16_t int16_t;
#endif

#ifndef INT32_T_DECLARED
#define INT32_T_DECLARED
typedef __int32_t int32_t;
#endif

#ifndef INT64_T_DECLARED
#define INT64_T_DECLARED
typedef __int64_t int64_t;
#endif

#ifndef UINTPTR_T_DECLARED
#define UINTPTR_T_DECLARED
typedef __uintptr_t uintptr_t;
#endif

#ifndef INTPTR_T_DECLARED
#define INTPTR_T_DECLARED
typedef __intptr_t intptr_t;
#endif

#ifndef UINTMAX_T_DECLARED
#define UINTMAX_T_DECLARED
typedef __uintmax_t uintmax_t;
#endif

#ifndef INTMAX_T_DECLARED
#define INTMAX_T_DECLARED
typedef __intmax_t intmax_t;
#endif

#endif /* __ANANAS_STDINT_H__ */
