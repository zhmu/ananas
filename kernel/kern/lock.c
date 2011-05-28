#include <ananas/types.h>
#include <ananas/lock.h>
#include <ananas/lib.h>
#include <ananas/schedule.h>
#include <machine/atomic.h>
#include <machine/interrupts.h>

void
spinlock_lock(spinlock_t* s)
{
	if (scheduler_activated())
		KASSERT(md_interrupts_save(), "interrups must be enabled");

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
	register_t state = md_interrupts_save();

	for(;;) {
		while(atomic_read(&s->sl_var) != 0)
			/* spin */ ;
		md_interrupts_disable();
		if (atomic_xchg(&s->sl_var, 1) == 0)
			break;
		/* Lock failed; restore interrupts and try again */
		md_interrupts_restore(state);
	}
	return state;
}

void
spinlock_unlock_unpremptible(spinlock_t* s, register_t state)
{
	spinlock_unlock(s);
	md_interrupts_restore(state);
}

/* vim:set ts=2 sw=2: */
