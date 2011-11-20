#include <sys/types.h>

#ifndef __POWERPC_ATOMIC_H__
#define __POWERPC_ATOMIC_H__

/* The atomic is the building block of all our locking preemtives */
typedef struct {
	int value;
} atomic_t;

static inline void atomic_set(atomic_t* a, int v)
{
	__asm __volatile(
"1:\n"
		"lwarx	%%r3, %%r0, %0\n"	/* load to dummy and reserve */
		"stwcx. %1, %%r0, %0\n"		/* store new value if still reserved */
		"bne-	1b\n"			/* loop if lost reservation */
	: : "r" (&a->value), "r" (v) : "%r3");
}

static inline int atomic_xchg(atomic_t* a, int v)
{
	register int m;
	__asm __volatile(
"1:\n"
		"lwarx	%0, %%r0, %1\n"	/* load and reserve */
		"stwcx. %2, %%r0, %1\n"	/* store new value if still reserved */
		"bne-	1b\n"		/* loop if lost reservation */
	: "=r" (m) : "r" (&a->value), "r" (v));
	return m;
		
}

static inline int atomic_read(atomic_t* a)
{
	register int v;
	__asm __volatile(
"1:\n"
		"lwarx	%0, %%r0, %1\n"	/* load and reserve */
		"stwcx.	%0, %%r0, %1\n"	/* store old value if still reserved */
		"bne-	1b\n"		/* loop if lost reservation */
	: "=r" (v) : "r" (&a->value));
	return v;
}

#endif /* __POWERPC_ATOMIC_H__ */
