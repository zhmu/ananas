#include "types.h"

#ifndef __AMD64_LOCK_H__
#define __AMD64_LOCK_H__

/* TODO */
typedef struct SPINLOCK* spinlock_t;

struct SPINLOCK {
	uint64_t	var;
};

#define spinlock_lock(x)
#define spinlock_unlock(x) 
#define spinlock_init(x)

#endif /* __AMD64_LOCK_H__ */
