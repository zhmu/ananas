#ifndef __AMD64_SETJMP_H__
#define __AMD64_SETJMP_H__

#include <machine/_types.h>

typedef struct {
	uint64_t	addr;
	uint64_t	rbx;
	uint64_t	rsp;
	uint64_t	rbp;
	uint64_t	r12;
	uint64_t	r13;
	uint64_t	r14;
	uint64_t	r15;
} jmp_buf[1];

#endif /* __AMD64_SETJMP_H__ */
