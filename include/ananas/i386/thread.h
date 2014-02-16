#include <sys/types.h>

#ifndef __I386_THREAD_H__
#define __I386_THREAD_H__

/* Details a 32-bit Task State Segment */
struct TSS {
	uint16_t	link;
	uint16_t	_reserved0;
	uint32_t	esp0;
	uint16_t	ss0;
	uint16_t	_reserved1;
	uint32_t	esp1;
	uint16_t	ss1;
	uint16_t	_reserved2;
	uint32_t	esp2;
	uint16_t	ss2;
	uint16_t	_reserved3;
	uint32_t	cr3;
	uint32_t	eip;
	uint32_t	eflags;
	uint32_t	eax;
	uint32_t	ecx;
	uint32_t	edx;
	uint32_t	ebx;
	uint32_t	esp;
	uint32_t	ebp;
	uint32_t	esi;
	uint32_t	edi;
	uint16_t	es;
	uint16_t	_reserved4;
	uint16_t	cs;
	uint16_t	_reserved5;
	uint16_t	ss;
	uint16_t	_reserved6;
	uint16_t	ds;
	uint16_t	_reserved7;
	uint16_t	fs;
	uint16_t	_reserved8;
	uint16_t	gs;
	uint16_t	_reserved9;
	uint16_t	ldt;
	uint16_t	flags;
#define TSS_FLAG_T	0x01			/* Debug Trap */
	uint16_t	iomap;
	uint32_t	iobitmap;
} __attribute__((packed));

/* Stored thread context - note that this must match cntx.S ! */
struct CONTEXT {
	uint32_t	eax;
	uint32_t	ebx;
	uint32_t	ecx;
	uint32_t	edx;
	uint32_t	esi;
	uint32_t	edi;
	uint32_t	ebp;
	uint32_t	esp;
	uint32_t	eip;
	uint32_t	cs;
	uint32_t	ds;
	uint32_t	es;
	uint32_t	fs;
	uint32_t	gs;
	uint32_t	ss;
	uint32_t	cr3;
	uint32_t	eflags;
	uint32_t	esp0;
	uint32_t	dr[8];
} __attribute__((packed));

struct FPUREGS {
	uint16_t	cw;		/* control word */
	uint16_t	reserved0;
	uint16_t	sw;		/* status word */
	uint16_t	reserved1;
	uint16_t	tw;		/* tag word */
	uint16_t	reserved2;
	uint32_t	ip;		/* instruction pointer */
	uint32_t	op_ip_sel;	/* opcode, ip selector */
	uint32_t	op;		/* operand */
	uint32_t	op_selector;	/* operand selector */
	uint8_t		r0[10];
	uint8_t		r1[10];
	uint8_t		r2[10];
	uint8_t		r3[10];
	uint8_t		r4[10];
	uint8_t		r5[10];
	uint8_t		r6[10];
	uint8_t		r7[10];
} __attribute__((packed));

/* i386-specific thread detais */
#define MD_THREAD_FIELDS \
	register_t	md_esp; \
	register_t	md_esp0; \
	register_t	md_eip; \
	register_t	md_cr3; \
	register_t	md_dr[8]; \
	register_t	md_arg1; /* XXX */ \
	register_t	md_arg2; /* XXX */ \
	struct FPUREGS	md_fpu_ctx __attribute__ ((aligned(16))); \
	void*		md_pagedir; \
	struct PAGE*	md_ustack_page; \
	void*		md_kstack;

void md_restore_ctx(struct CONTEXT* ctx);

#define md_cpu_relax() \
	__asm __volatile("hlt")

#endif /* __I386_THREAD_H__ */
