#ifndef __AMD64_INTERRUPTS_H__
#define __AMD64_INTERRUPTS_H__

#define MAX_IRQS 256

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
		"pushfq\n"
		"popq %%rdx\n"
		/* mask interrupt flag and re-enable flag if needed */
		"andq $~0x200, %%rdx\n"
		"orl %0, %%edx\n"
		/* activate new eflags */
		"pushq %%rdx\n"
		"popfq\n"
	: : "a" (enabled) : "%rdx");
}

static inline int md_interrupts_save()
{
	register int r;
	__asm __volatile(
		/* get eflags */
		"pushfq\n"
		"popq %%rdx\n"
		/* but only the interrupt flag */
		"andq $0x200, %%rdx\n"
		"movl %%edx, %0"
	: "=r" (r) : : "%rdx");
	return r;
}

static inline int md_interrupts_save_and_disable()
{
	int status = md_interrupts_save();
	md_interrupts_disable();
	return status;
}

#endif /* __AMD64_INTERRUPTS_H__ */
