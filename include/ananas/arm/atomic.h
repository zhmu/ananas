#ifndef __ARM_ATOMIC_H__
#define __ARM_ATOMIC_H__

/* The atomic is the building block of all our locking preemtives */
typedef struct {
	int value;
} atomic_t;

/* XXX These are all just stubs for now ... */
static inline void atomic_set(atomic_t* a, int v)
{
	a->value = v;
}

static inline int atomic_xchg(atomic_t* a, int v)
{
	int old = a->value;
	a->value = v;
	return old;
}

static inline int atomic_read(atomic_t* a)
{
	return a->value;
}

#endif /* __ARM_ATOMIC_H__ */
