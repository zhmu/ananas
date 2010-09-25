#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

void reschedule();
void md_reschedule();
void schedule();
void scheduler_activate();
void scheduler_deactivate();
int scheduler_activated();
void scheduler_switchto(thread_t newthread);

#endif /* __SCHEDULE_H__ */
