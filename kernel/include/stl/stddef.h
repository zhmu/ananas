#pragma once

typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;

#define unsigned signed
typedef __SIZE_TYPE__ ssize_t;
#undef unsigned

#define offsetof __builtin_offsetof

#ifdef __cplusplus
# define NULL 0
#else
# define NULL ((void*)0)
#endif
