#include <ananas/types.h>
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
	uint16_t	fcw;		/* control word */
	uint16_t	fsw;		/* status word */
	uint16_t	ftw;		/* tag word */
	uint16_t	fop;		/* operator */
	uint32_t	fip;		/* instruction pointer */
	uint16_t	cs;		/* */
	uint16_t	_res0;		/* reserved */
	uint32_t	fpudp;		/* */
	uint16_t	ds;		/* */
	uint16_t	_res1;		/* */
	uint32_t	mxcsr;		/* */
	uint32_t	mxcsr_mask;	/* */
	uint8_t		st0[16];
	uint8_t		st1[16];
	uint8_t		st2[16];
	uint8_t		st3[16];
	uint8_t		st4[16];
	uint8_t		st5[16];
	uint8_t		st6[16];
	uint8_t		st7[16];
	uint8_t		xmm0[16];
	uint8_t		xmm1[16];
	uint8_t		xmm2[16];
	uint8_t		xmm3[16];
	uint8_t		xmm4[16];
	uint8_t		xmm5[16];
	uint8_t		xmm6[16];
	uint8_t		xmm7[16];
	uint8_t		xmm8[16];
	uint8_t		xmm9[16];
	uint8_t		xmm10[16];
	uint8_t		xmm11[16];
	uint8_t		xmm12[16];
	uint8_t		xmm13[16];
	uint8_t		xmm14[16];
	uint8_t		xmm15[16];
	uint8_t		_res2[16];
	uint8_t		_res3[16];
	uint8_t		_res4[16];
	uint8_t		_res5[16];
	uint8_t		_res6[16];
	uint8_t		_res7[16];
} __attribute__((packed));

/* amd64-specific thread details */
#define MD_THREAD_FIELDS \
	register_t	md_rsp; \
	register_t	md_rsp0; \
	register_t	md_rip; \
	register_t	md_cr3; \
	register_t	md_arg1 /* XXX */; \
	register_t	md_arg2 /* XXX */; \
	struct PAGE* md_kstack_page; \
	struct FPUREGS	md_fpu_ctx __attribute__ ((aligned(16))); \
	void*		md_stack; \
	void*		md_kstack;

#define md_cpu_relax() \
	__asm __volatile("hlt")

#endif /* __AMD64_THREAD_H__ */
