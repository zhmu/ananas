#include <ananas/types.h>
#include <ananas/lock.h>

/*
 * The spinlock implementation is based on IA-32 Intel Architecture Software
 * Developer's Manual, Volume 3: System programming guide, section 
 * 7.6.9.1: "Use the pause instruction in spin-wait loops".
 */
void
md_spinlock_lock(spinlock_t s)
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
md_spinlock_unlock(spinlock_t s)
{
	__asm(
		"movl	$0, (%%eax)\n"
	: : "a" (&s->var));
}

void
md_spinlock_init(spinlock_t s)
{
	s->var = 0;
}

/* vim:set ts=2 sw=2: */
