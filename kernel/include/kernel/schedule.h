#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include <ananas/types.h>
#include "kernel/list.h"

struct Thread;

struct SCHED_PRIV {
	Thread* sp_thread;	/* Backreference to the thread */
	LIST_FIELDS(struct SCHED_PRIV);
};

LIST_DEFINE(SCHEDULER_QUEUE, struct SCHED_PRIV);

void scheduler_add(Thread& t);
void scheduler_remove(Thread& t);

void schedule();
#define reschedule schedule
void scheduler_activate();
void scheduler_deactivate();
int scheduler_activated();
void scheduler_launch();

/* Initializes the scheduler-specific part for a given thread */
void scheduler_init_thread(Thread& t);

/* Register a thread for scheduling */
void scheduler_add_thread(Thread& t);

/* Unregister a thread for scheduling */
void scheduler_remove_thread(Thread& t);

/* Exits a thread - removes it from the runqueue in a safe manner */
void scheduler_exit_thread(Thread& t);

#endif /* __SCHEDULE_H__ */
