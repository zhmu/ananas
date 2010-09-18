#ifndef __POWERPC_SETJMP_H__
#define __POWERPC_SETJMP_H__

#include <machine/_types.h>

typedef struct {
	uint32_t	lr;
	uint32_t	r1;
} jmp_buf[1];

#endif /* __POWERPC_SETJMP_H__ */
