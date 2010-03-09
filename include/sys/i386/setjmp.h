#ifndef __I386_SETJMP_H__
#define __I386_SETJMP_H__

#include <types.h>

typedef struct {
	uint32_t	addr;
	uint32_t	ebx;
	uint32_t	esp;
	uint32_t	ebp;
	uint32_t	esi;
	uint32_t	edi;
} jmp_buf[1];

#endif /* __I386_SETJMP_H__ */
