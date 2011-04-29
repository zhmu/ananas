#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include <ananas/types.h>
#include <ananas/dqueue.h>

struct SCHED_PRIV {
	struct THREAD* sp_thread;	/* Backreference to the thread */
	DQUEUE_FIELDS(struct SCHED_PRIV);
};

DQUEUE_DEFINE(SCHEDULER_QUEUE, struct SCHED_PRIV);

void scheduler_add(struct THREAD* t);
void scheduler_remove(struct THREAD* t);

void schedule();
void md_reschedule();
#define reschedule md_reschedule
void scheduler_activate();
void scheduler_deactivate();
int scheduler_activated();

/* Initializes the scheduler-specific part for a given thread */
void scheduler_init_thread(struct THREAD* t);

/* Register a thread for scheduling */
void scheduler_add_thread(struct THREAD* t);

/* Unregister a thread for scheduling */
void scheduler_remove_thread(struct THREAD* t);

#endif /* __SCHEDULE_H__ */
