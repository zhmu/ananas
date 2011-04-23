#include <ananas/types.h>
#include <ananas/lock.h>

void
md_spinlock_lock(spinlock_t s)
{
}

void
md_spinlock_unlock(spinlock_t s)
{
}

void
md_spinlock_init(spinlock_t s)
{
}

register_t
spinlock_lock_unpremptible(spinlock_t s)
{
	return 0;
}

void
spinlock_unlock_unpremptible(spinlock_t s, register_t state)
{
}

/* vim:set ts=2 sw=2: */
