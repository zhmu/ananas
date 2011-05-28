#ifndef __AMD64_ATOMIC_H__
#define __AMD64_ATOMIC_H__

/* The atomic is the building block of all our locking preemtives */
typedef struct {
	int value;
} atomic_t;

static inline void atomic_set(atomic_t* a, int v)
{
	*(volatile int*)&a->value = v;
}

static inline int atomic_xchg(atomic_t* a, int v)
{
	register int m = v;
	__asm __volatile(
		"xchg (%1), %0"
	: "+r" (m) : "r" (&a->value));
	return m;
		
}

static inline int atomic_read(atomic_t* a)
{
	return *(volatile int*)&a->value;
}

#endif /* __AMD64_ATOMIC_H__ */
