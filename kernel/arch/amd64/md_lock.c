#include <ananas/types.h>
#include <ananas/lock.h>

/*
 * The spinlock implementation is based on IA-32 Intel Architecture Software
 * Developer's Manual, Volume 3: System programming guide, section 
 * 7.6.9.1: "Use the pause instruction in spin-wait loops".
 */
void
spinlock_lock(spinlock_t s)
{
#if 0
	__asm(
"l1:\n"
		"cmpl $0, (%%rax)\n"
		"je		l2\n"
		"pause\n"
		"jmp	l1\n"
"l2:\n"
		"movq	$1, %%rbx\n"
		"xchg	(%%rax), %%rbx\n"
		"cmpq	$0, %%rbx\n"
		"jne	l1\n"
	: : "a" (&s->var) : "rbx");
#endif
}

void
spinlock_unlock(spinlock_t s)
{
#if 0
	__asm(
		"movl	$0, (%%rax)\n"
	: : "a" (&s->var));
#endif
}

void
spinlock_init(spinlock_t s)
{
	s->var = 0;
}

register_t
spinlock_lock_unpremptible(spinlock_t s)
{
	register_t state;
	__asm __volatile("pushfq; popq %%rax" : "=a" (state));
	spinlock_lock(s);
	__asm __volatile("cli"); /* disable interrupts after acquiring spinlock! */
	return state;
}

void
spinlock_unlock_unpremptible(spinlock_t s, register_t state)
{
	spinlock_unlock(s);
	__asm __volatile("pushq %%rax; popfq" : : "a" (state));
}

/* vim:set ts=2 sw=2: */
