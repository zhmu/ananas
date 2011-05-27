#include <ananas/types.h>
#include <ananas/lock.h>
#include <ananas/lib.h>
#include <ananas/schedule.h>
#include <machine/atomic.h>
#include <machine/macro.h>

#define LOAD_LOCK_VALUE \
		"movl	4(%%ebp), %%ebx\n"	/* return eip */

/*
 * The spinlock implementation is based on IA-32 Intel Architecture Software
 * Developer's Manual, Volume 3: System programming guide, section 
 * 7.6.9.1: "Use the pause instruction in spin-wait loops".
 */
void
spinlock_lock(spinlock_t* s)
{
	if (scheduler_activated())
		KASSERT(GET_EFLAGS() & 0x200, "interrups must be enabled");

	 for(;;) {
		while(atomic_read(&s->sl_var) != 0)
			/* spin */ ;
		if (atomic_xchg(&s->sl_var, 1) == 0)
			break;
	}
}

void
spinlock_unlock(spinlock_t* s)
{
	if (atomic_xchg(&s->sl_var, 0) == 0)
		panic("spinlock %p was not locked", s);
}

void
spinlock_init(spinlock_t* s)
{
	atomic_set(&s->sl_var, 0);
}

register_t
spinlock_lock_unpremptible(spinlock_t* s)
{
	int state = GET_EFLAGS();

	 for(;;) {
		while(atomic_read(&s->sl_var) != 0)
			/* spin */ ;
		__asm __volatile("cli");
		if (atomic_xchg(&s->sl_var, 1) == 0)
			break;
		/* Lock failed; restore interrupts and try again */
		__asm __volatile("pushl %%eax; popfl" : : "a" (state));
	}
	return state;
}

void
spinlock_unlock_unpremptible(spinlock_t* s, register_t state)
{
	spinlock_unlock(s);
	__asm __volatile("pushl %%eax; popfl" : : "a" (state));
}

/* vim:set ts=2 sw=2: */
