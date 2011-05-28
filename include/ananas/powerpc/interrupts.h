#ifndef __POWERPC_INTERRUPTS_H__
#define __POWERPC_INTERRUPTS_H__

#include <ananas/types.h>
#include <machine/macro.h>

#define MAX_IRQS 16 /* XXX wild guess */

#define INT_SR	0x0100			/* System Reset */
#define INT_MC	0x0200			/* Machine Check */
#define INT_DS	0x0300			/* Data Storage */
#define INT_IS	0x0400			/* Instruction Storage */
#define INT_EXT	0x0500			/* External */
#define INT_A	0x0600			/* Alignment */
#define INT_P	0x0700			/* Program */
#define INT_FPU	0x0800			/* Floating-Point Unavailable */
#define INT_DEC	0x0900			/* Decrementer */
#define _INT_R1	0x0a00			/* Reserved */
#define _INT_R2	0x0b00			/* Reserved */
#define INT_SC	0x0c00			/* System Call */
#define INT_T	0x0d00			/* Trace */
#define INT_FPA	0x0e00			/* Floating-Point Assist */

void exception_init();

static inline void md_interrupts_enable()
{
	__asm __volatile(
		"mfmsr	%%r1\n"
		"ori	%%r1, %%r1, 0x8000\n"		/* set EE */
		"mtmsr	%%r1\n"
	: : : "%r1");
}

static inline void md_interrupts_disable()
{
	__asm __volatile(
		"mfmsr	%%r1\n"
		"rlwinm	%%r1, %%r1, 0, 17, 15\n"	/* clears bit 16 (EE) */
		"mtmsr	%%r1\n"
	: : : "%r1");
}

static inline void md_interrupts_restore(int enabled)
{
	__asm __volatile(
		"mfmsr	%%r1\n"
		"ori	%%r1, %%r1, %0\n"
		"mtmsr	%%r1\n"
	: : "r" (enabled) : "%r1");
}

static inline int md_interrupts_save()
{
	register int r;
	__asm __volatile(
		"mfmsr	%0\n"
		"andi.	%0, %0, 0x8000\n"
	: "=r" (r));
	return r;
}

static inline int md_interrupts_save_and_disable()
{
	int status = md_interrupts_save();
	md_interrupts_disable();
	return status;
}

#endif /* __POWERPC_INTERRUPTS_H__ */
