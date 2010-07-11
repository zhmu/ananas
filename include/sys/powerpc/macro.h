#ifndef __POWERPC_MACRO_H__
#define __POWERPC_MACRO_H__

inline static uint32_t
rdmsr()
{
	register uint32_t msr;
	__asm __volatile("mfmsr %0\n" : "=r" (msr));
	return msr;
}

inline static void
wrmsr(uint32_t msr)
{
	__asm __volatile(
		"mtmsr %0\n"
		"isync\n"
	: : "r" (msr));
}

inline static void
mtsrin(uint32_t sr, uint32_t val)
{
	__asm __volatile(
		"mtsrin %0,%1\n"
		"isync\n"
	: : "r" (val), "r" (sr));
}

#define TRACE(x...) \
	kprintf(x)

#endif /* __POWERPC_MACRO_H__ */
