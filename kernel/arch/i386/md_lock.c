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
	__asm(
"l1:\n"
		"cmpl $0, (%%eax)\n"
		"je		l2\n"
		"pause\n"
		"jmp	l1\n"
"l2:\n"
		"movl	$1, %%ebx\n"
		"xchg	(%%eax), %%ebx\n"
		"cmpl	$0, %%ebx\n"
		"jne	l1\n"
	: : "a" (&s->var) : "ebx");
}

void
spinlock_unlock(spinlock_t s)
{
	__asm(
		"movl	$0, (%%eax)\n"
	: : "a" (&s->var));
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
	__asm __volatile("pushfl; pop %%eax" : "=a" (state));
	spinlock_lock(s);
	__asm __volatile("cli"); /* disable interrupts after acquiring spinlock! */
	return state;
}

void
spinlock_unlock_unpremptible(spinlock_t s, register_t state)
{
	spinlock_unlock(s);
	__asm __volatile("push %%eax; popfl" : : "a" (state));
}

/* vim:set ts=2 sw=2: */
