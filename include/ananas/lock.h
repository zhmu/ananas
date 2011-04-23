#ifndef __LOCK_H__
#define __LOCK_H__

#include <machine/lock.h>

typedef struct SPINLOCK* spinlock_t;

/* Ordinary spinlocks which can be preempted at any time */
void spinlock_lock(spinlock_t l);
void spinlock_unlock(spinlock_t l);
void spinlock_init(spinlock_t l);

/* Unpremptible spinlocks disable the interrupt flag while they are active */
register_t spinlock_lock_unpremptible(spinlock_t l);
void spinlock_unlock_unpremptible(spinlock_t l, register_t state);


#endif /* __LOCK_H__ */
