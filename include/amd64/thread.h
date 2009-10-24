#include "types.h"

#ifndef __AMD64_THREAD_H__
#define __AMD64_THREAD_H__

/* Stored thread context - note that this must match cntx.S ! */
struct CONTEXT {
	uint64_t	/* 00 */ rax;
	uint64_t	/* 04 */ rbx;
	uint64_t	/* 08 */ rcx;
	uint64_t	/* 0c */ rdx;
	uint64_t	/* 10 */ rsi;
	uint64_t	/* 14 */ rdi;
	uint64_t	/* 18 */ rbp;
	uint64_t	/* 1c */ rsp;
	uint64_t	/* 20 */ rip;
	uint64_t	/* 24 */ cs;
	uint64_t	/* 28 */ ds;
	uint64_t	/* 2c */ es;
	uint64_t	/* 30 */ fs;
	uint64_t	/* 34 */ gs;
	uint64_t	/* 38 */ ss;
	uint64_t	/* 3c */ cr3;
	uint64_t	/* 40 */ eflags;
	uint64_t	/* 44 */ rsp0;
};

struct MD_THREAD {
	struct CONTEXT	ctx;
};

#endif /* __I386_THREAD_H__ */
