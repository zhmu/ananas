#ifndef __ARM_STDDEF_H__
#define __ARM_STDDEF_H__

#ifdef __SIZE_TYPE__
typedef __SIZE_TYPE__ size_t;
#else
typedef unsigned int	size_t;
#endif

#ifdef __PTRDIFF_TYPE__
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#else
typedef unsigned int	ptrdiff_t;
#endif

#endif /* __ARM_STDDEF_H__ */
