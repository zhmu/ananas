#ifndef __I386_INTERRUPTS_H__
#define __I386_INTERRUPTS_H__

#define MAX_IRQS 256

#define SYSCALL_INT 0x30

#ifndef ASM
/* Syscall interrupt code */
extern void* syscall_int;

static inline void md_interrupts_enable()
{
	__asm __volatile("sti");
}

static inline void md_interrupts_disable()
{
	__asm __volatile("cli");
}

static inline void md_interrupts_restore(int enabled)
{
	__asm __volatile(
		/* get eflags in %edx */
		"pushfl\n"
		"popl %%edx\n"
		/* mask interrupt flag and re-enable flag if needed */
		"andl $~0x200, %%edx\n"
		"orl %0, %%edx\n"
		/* activate new eflags */
		"pushl %%edx\n"
		"popfl\n"
	: : "a" (enabled) : "%edx");
}

static inline int md_interrupts_save()
{
	int r;
	__asm __volatile(
		/* get eflags */
		"pushfl\n"
		"popl %0\n"
		/* but only the interrupt flag */
		"andl $0x200, %0\n"
	: "=r" (r));
	return r;
}

static inline int md_interrupts_save_and_disable()
{
	int status = md_interrupts_save();
	md_interrupts_disable();
	return status;
}
#endif

#endif /* __I386_INTERRUPTS_H__ */
