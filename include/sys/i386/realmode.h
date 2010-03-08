#ifndef __I386_REALMODE_H__
#define __I386_REALMODE_H__

struct realmode_regs {
	uint32_t	edi;
	uint32_t	esi;
	uint32_t	ebp;
	uint32_t	_esp;
	uint32_t	ebx;
	uint32_t	edx;
	uint32_t	ecx;
	uint32_t	eax;
} __attribute__((packed));

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

void realmode_call_int(unsigned char num, struct realmode_regs* r);

/*
 * The realmode code/data descriptors are relative to realmode16_code.
 * Note that it does not matter that we have relocated addresses here,
 * because these will cancel eachother out.
 */
#define realmode_get_storage_offs() \
	((addr_t)&realmode_store - (addr_t)&realmode16_code)

extern void realmode16_stub();
extern void realmode16_code();
extern uint8_t* realmode_store;

#endif /* __I386_REALMODE_H__ */
