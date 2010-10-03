#include <sys/types.h>

#ifndef __I386_FRAME_H__
#define __I386_FRAME_H__

struct STACKFRAME {
	uint32_t	sf_trapno;
	uint32_t	sf_eax;
	uint32_t	sf_ebx;
	uint32_t	sf_ecx;
	uint32_t	sf_edx;
	uint32_t	sf_ebp;
	uint32_t	sf_esp;
	uint32_t	sf_edi;
	uint32_t	sf_esi;

	uint32_t	sf_ds;
	uint32_t	sf_es;
	uint32_t	sf_fs;
	uint32_t	sf_gs;

	/* added by the hardware */
	register_t	sf_errnum;
	register_t	sf_eip;
	register_t	sf_cs;
	register_t	sf_eflags;
	register_t	sf_sp;
	register_t	sf_ss;
};

#define EFLAGS_CR		(1 << 0)	/* Carry Flag */
#define EFLAGS_PF		(1 << 1)	/* Parity Flag */
#define EFLAGS_AF		(1 << 4)	/* Auxiliary Carry Flag */
#define EFLAGS_ZF		(1 << 6)	/* Zero Flag */
#define EFLAGS_SF		(1 << 7)	/* Sign Flag */
#define EFLAGS_TF		(1 << 8)	/* Trap Flag */
#define EFLAGS_IF		(1 << 9)	/* Interrupt Enable Flag */
#define EFLAGS_DF		(1 << 10)	/* Direction Flag */
#define EFLAGS_OF		(1 << 11)	/* Overflow Flag */
#define EFLAGS_NT		(1 << 14)	/* Nested Task */
#define EFLAGS_RF		(1 << 16)	/* Resume Flag */
#define EFLAGS_VM		(1 << 17)	/* Virtual-8086 Mode */
#define EFLAGS_AC		(1 << 18)	/* Alignment Check */
#define EFLAGS_VIF		(1 << 19)	/* Virtual Interrupt Flag */
#define EFLAGS_VIP		(1 << 20)	/* Virtual Interrupt Pending */
#define EFLAGS_ID		(1 << 21)	/* ID flag */

#endif /* __I386_FRAME_H__ */
