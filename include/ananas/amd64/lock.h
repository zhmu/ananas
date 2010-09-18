#include <ananas/types.h>

#ifndef __AMD64_LOCK_H__
#define __AMD64_LOCK_H__

typedef struct SPINLOCK* spinlock_t;

struct SPINLOCK {
	uint64_t	var;
};

void md_spinlock_lock(spinlock_t l);
void md_spinlock_unlock(spinlock_t l);
void md_spinlock_init(spinlock_t l);

#define spinlock_lock(x) md_spinlock_lock(x)
#define spinlock_unlock(x) md_spinlock_unlock(x)
#define spinlock_init(x) md_spinlock_init(x)


#endif /* __AMD64_LOCK_H__ */
