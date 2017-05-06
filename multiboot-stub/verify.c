#include "verify.h"
#include "io.h"

#define EFLAG_LM	(1 << 29)
#define EFLAG_NX	(1 << 20)

static inline int
supports_cpuid()
{
	/*
	 * Try to flip bit 21 (cpuid supported) - if this works, we're running
	 * on really old stuff.
	 */
	int r;
	__asm __volatile(
		/* Get current flags in %ebx */
		"pushfl\n"
		"popl	%%eax\n"
		"movl	%%eax,%%ebx\n"
		/* Try to flip CPUID bit and get flags in %eax */
		"xorl	$(1 << 21),%%eax\n"
		"pushl	%%eax\n"
		"popfl\n"
		"pushfl\n"
		"popl	%%eax\n"
		/*
		 * If %eax = %ebx, we couldn't change the CPUID bit and thus
		 * CPUID is not supported.
		 */
		"xor	%%ebx,%%eax\n"
	: "=a" (r));
	return r;
}

static inline unsigned int
get_extended_flags()
{
	unsigned int r;
	__asm __volatile(
		/* Ask for extended flags */
		"movl	$0x80000001,%%eax\n"
		"xorl	%%edx,%%edx\n"
		"cpuid\n"
		"movl	%%edx, %%eax\n"
	: "=a" (r));
	return r;
}

void
verify_platform()
{
	if (!supports_cpuid())
		panic("CPUID not supported");

	unsigned int ef = get_extended_flags();
	if ((ef & EFLAG_LM) == 0)
		panic("not a 64 bit capable CPU");
	if ((ef & EFLAG_NX) == 0)
		panic("no-execute not supported");
}
