#include <ananas/types.h>
#include <ananas/lock.h>
#include <ananas/lib.h>

/*
 * The spinlock implementation is based on IA-32 Intel Architecture Software
 * Developer's Manual, Volume 3: System programming guide, section 
 * 7.6.9.1: "Use the pause instruction in spin-wait loops".
 */
void
spinlock_lock(spinlock_t s)
{
	__asm __volatile(
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
	uint32_t oldval;
	__asm __volatile(
		"xorl %%ebx,%%ebx\n"
		"xchg	(%%eax), %%ebx\n"
	: "=b" (oldval) : "a" (&s->var));
	KASSERT(oldval != 0, "unlocking spinlock 0x%p that isn't locked", s);
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
	__asm __volatile(
		"pushfl\n"
		"pop %%edx\n"
"ll1:\n"
		"cmpl $0, (%%eax)\n"
		"je		ll2\n"
		"pushl %%edx\n"
		"popfl\n"
		"pause\n"
		"jmp	ll1\n"
"ll2:\n"
		/*
	 	 * Attempt to lock spinlock; we must disable the interrupts as we
		 * try because we must not be preempted while holding the lock.
		 */
		"cli\n"
		"movl	$1, %%ebx\n"
		"xchg	(%%eax), %%ebx\n"
		"cmpl	$0, %%ebx\n"
		"je	ll3\n"
		/* Lock failed; restore interrupts and try again */
		"pushl %%edx\n"
		"popfl\n"
		"jmp ll2\n"
"ll3:\n"
	: "=d" (state) : "a" (&s->var));
	return state;
}

void
spinlock_unlock_unpremptible(spinlock_t s, register_t state)
{
	spinlock_unlock(s);
	__asm __volatile("push %%eax; popfl" : : "a" (state));
}

/* vim:set ts=2 sw=2: */
