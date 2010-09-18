#ifndef __X86_EXCEPTIONS_H__
#define __X86_EXCEPTIONS_H__

#define EXC_DE	0	/* Division by zero */
#define EXC_DB	1	/* Debug exception */
#define EXC_NMI	2	/* NMI */
#define EXC_BP	3	/* Breakpoint exception */
#define EXC_OF	4	/* Overflow exception */
#define EXC_BR	5	/* Bound range exceeded exception */
#define EXC_UD	6	/* Invalid Opcode exception */
#define EXC_NM	7	/* Device not available exception */
#define EXC_DF	8	/* Double fault exception */
#define EXC_TS	10	/* Invalid TSS exception */
#define EXC_NP	11	/* Segment not present exception */
#define EXC_SS	12	/* Stack fault exception */
#define EXC_GP	13	/* General Protection Fault exception */
#define EXC_PF	14	/* Page Fault exception */
#define EXC_MF	16	/* FPU Floating-Point error */
#define EXC_AC	17	/* Alignment Check Exception */
#define EXC_MC	18	/* Machine Check Exception */
#define EXC_XF	19	/* SIMD Floating-Point Exception */

const char* x86_exception_name(int num);

#endif /* __X86_EXCEPTIONS_H__ */
