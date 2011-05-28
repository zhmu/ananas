#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include <machine/atomic.h>

typedef struct {
	atomic_t		sl_var;
} spinlock_t;

#endif /* __SPINLOCK_H__ */
