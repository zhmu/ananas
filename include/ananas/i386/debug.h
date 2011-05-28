#ifndef __I386_DEBUG_H__
#define __I386_DEBUG_H__

#include <ananas/types.h>
#include <ananas/thread.h>

/* Debug register 6 flags */
#define DR6_B0		(1 << 0)	/* Breakpoint condition 0 detected */
#define DR6_B1		(1 << 1)	/* Breakpoint condition 0 detected */
#define DR6_B2		(1 << 2)	/* Breakpoint condition 0 detected */
#define DR6_B3		(1 << 3)	/* Breakpoint condition 0 detected */
#define DR6_BD		(1 << 13)	/* Debug register access detected */
#define DR6_BS		(1 << 14)	/* Single step */
#define DR6_BT		(1 << 15)	/* Task switched */

/* Debug register 7 settings */
#define DR7_L(x)	(1<<(2*(x)))		/* Local breakpoint x enable */
#define DR7_G(x)	(1<<((2*(x))+1))	/* Global breakpoint x enable */
#define DR7_LEN(n,x)	((n)<<((4*(x))+18))	/* Length n for breakpoint x */
#define DR7_RW(n,x)	((n)<<((4*(x))+16))	/* Read/write n for breakpoint x */

/* Convenience macro's */
#define DR7_L0		DR7_L(0)	/* Local breakpoint 0 enable */
#define DR7_G0		DR7_G(0)	/* Global breakpoint 0 enable */
#define DR7_L1		DR7_L(1)	/* Local breakpoint 1 enable */
#define DR7_G1		DR7_G(1)	/* Global breakpoint 1 enable */
#define DR7_L2		DR7_L(2)	/* Local breakpoint 2 enable */
#define DR7_G2		DR7_G(2)	/* Global breakpoint 2 enable */
#define DR7_L3		DR7_L(3)	/* Local breakpoint 3 enable */
#define DR7_G3		DR7_G(3)	/* Global breakpoint 3 enable */
#define DR7_LE		(1 << 8)	/* Local exact breakpoint enable */
#define DR7_GE		(1 << 9)	/* Global exact breakpoint enable */
#define DR7_GD		(1 << 13)	/* General detect enable */
#define DR7_RW0(x)	DR7_RW(x,0)	/* Read/write fields for breakpoint 0 */
#define DR7_LEN0(x)	DR7_LEN(x,0)	/* Length for breakpoint 0 */
#define DR7_RW1(x)	DR7_RW(x,1)	/* Read/write fields for breakpoint 1 */
#define DR7_LEN1(x)	DR7_LEN(x,1)	/* Length for breakpoint 1 */
#define DR7_RW2(x)	DR7_RW(x,2)	/* Read/write fields for breakpoint 2 */
#define DR7_LEN2(x)	DR7_LEN(x,2)	/* Length for breakpoint 2 */
#define DR7_RW3(x)	DR7_RW(x,3)	/* Read/write fields for breakpoint 3 */
#define DR7_LEN3(x)	DR7_LEN(x,3)	/* Length for breakpoint 3 */
#define DR7_CLR(x) \
	(DR7_L(x) | DR7_G(x) | DR7_LEN(3,x) | DR7_RW(3,x))	/* Clear breakpoint x */

/* RWx values */
#define DR7_RW_X	0		/* Instruction execution only */
#define DR7_RW_W	1		/* Data write only */
#define DR7_RW_IO	2		/* I/O read/write only */
#define DR7_RW_RW	3		/* Data read/write only */

/* LENx values */
#define DR7_LEN_1	0		/* 1 byte */
#define DR7_LEN_2	1		/* 2 bytes */
#define DR7_LEN_4	3		/* 4 bytes */

/* Macro's for direct manipulation */
#define DR_GET(n)	({ uint32_t x; __asm __volatile("movl %%dr"#n", %0" : "=r" (x)); x; })
#define DR_SET(n,x)	__asm __volatile("movl %0, %%db" #n : : "r" (x))

/* Amount of debug registers */
#define DR_AMOUNT	4

int md_thread_set_dr(thread_t* t, int len, int rw, addr_t addr);
void md_thread_clear_dr(thread_t* t, int n);

#endif /* __I386_DEBUG_H__ */
