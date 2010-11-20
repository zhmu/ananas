#ifndef __POWERPC_INTERRUPTS_H__
#define __POWERPC_INTERRUPTS_H__

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

#endif /* __POWERPC_INTERRUPTS_H__ */
