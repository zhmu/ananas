#include "pcpu.h"

volatile static struct THREAD*
pcpu_curthread()
{
	uint32_t o;
	__asm(
		"mov %%fs:%1, %0\n"
		: "=r" (o) : "m" (*(int*)__builtin_offsetof(struct PCPU, curthread)));
	return (struct THREAD*)o;
}

static void
pcpu_curthread_set(struct THREAD* t)
{
	__asm(
			"mov %0, %%fs:%1\n"
		: : "r" (t), "m" (*(int*)__builtin_offsetof(struct PCPU, curthread)));
}

