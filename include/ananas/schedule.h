#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include <ananas/types.h>
#include <ananas/dqueue.h>

struct SCHED_PRIV {
	thread_t* sp_thread;	/* Backreference to the thread */
	DQUEUE_FIELDS(struct SCHED_PRIV);
};

DQUEUE_DEFINE(SCHEDULER_QUEUE, struct SCHED_PRIV);

void scheduler_add(thread_t* t);
void scheduler_remove(thread_t* t);

void schedule();
#define reschedule schedule
void scheduler_activate();
void scheduler_deactivate();
int scheduler_activated();
void scheduler_launch();

/* Initializes the scheduler-specific part for a given thread */
void scheduler_init_thread(thread_t* t);

/* Register a thread for scheduling */
void scheduler_add_thread(thread_t* t);

/* Unregister a thread for scheduling */
void scheduler_remove_thread(thread_t* t);

/* Exits a thread - removes it from the runqueue in a safe manner */
void scheduler_exit_thread(thread_t* t);

#endif /* __SCHEDULE_H__ */
