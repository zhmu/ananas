#ifndef __LOADER_X86_H__
#define __LOADER_X86_H__

struct REALMODE_REGS {
	uint32_t	eax;
	uint32_t	ebx;
	uint32_t	ecx;
	uint32_t	edx;
	uint32_t	ebp;
	uint32_t	esi;
	uint32_t	edi;
	uint32_t	esp;
	uint16_t	ds;
	uint16_t	es;
	uint16_t	ss;
	uint32_t	eflags;
#define EFLAGS_CF	(1 << 0)		/* Carry flag */
#define EFLAGS_PF	(1 << 2)		/* Parity flag */
#define EFLAGS_AF	(1 << 4)		/* Auxiliary carry flag */
#define EFLAGS_ZF	(1 << 6)		/* Zero flag */
#define EFLAGS_SF	(1 << 7)		/* Sign flag */
#define EFLAGS_IF	(1 << 9)		/* Interrupt flag */
#define EFLAGS_DF	(1 << 10)		/* Direction flag */
	uint8_t		interrupt;
	uint16_t	cs;
	uint16_t	ip;
} __attribute__((packed));

extern void  realcall();
extern void* rm_regs;
extern void* entry;
extern void* rm_buffer;
extern void* rm_stack;
#define REALMODE_BUFFER ((uint32_t)&rm_buffer - (uint32_t)&entry)

void x86_realmode_init(struct REALMODE_REGS* regs);
void x86_realmode_push16(struct REALMODE_REGS* regs, uint16_t value);
void x86_realmode_call(struct REALMODE_REGS* regs);

#endif /* __LOADER_X86_H__ */
