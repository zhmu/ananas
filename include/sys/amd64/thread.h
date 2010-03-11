#include <sys/types.h>
#include <machine/frame.h>

#ifndef __AMD64_THREAD_H__
#define __AMD64_THREAD_H__

/* Details a 64-bit Task State Segment */
struct TSS {
	uint32_t	_reserved0;
	uint64_t	rsp0;
	uint64_t	rsp1;
	uint64_t	rsp2;
	uint64_t	_reserved1;
	uint64_t	ist1;
	uint64_t	ist2;
	uint64_t	ist3;
	uint64_t	ist4;
	uint64_t	ist5;
	uint64_t	ist6;
	uint64_t	ist7;
	uint64_t	_reserved2;
	uint32_t	_reserved3;
	uint32_t	iomap_base;
} __attribute__((packed));

struct FPUREGS {
	uint8_t		_reg[512];
} __attribute__((packed));

/* amd64 thread context */
struct CONTEXT {
	struct		STACKFRAME sf;
	struct		FPUREGS fpu __attribute__ ((aligned(16)));
	uint64_t	pml4;
};

struct MD_THREAD {
	struct CONTEXT	ctx;

	void*		pml4;
	void*		stack;
	void*		kstack;
};

void md_restore_ctx(struct CONTEXT* ctx);

#endif /* __I386_THREAD_H__ */
