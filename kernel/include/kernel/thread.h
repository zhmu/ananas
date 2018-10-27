#ifndef __THREAD_H__
#define __THREAD_H__

#include <ananas/types.h>
#include <ananas/util/list.h>
#include "kernel/page.h"
#include "kernel/schedule.h" // for SchedulerPriv
#include "kernel/signal.h" // for ThreadSpecificData
#include "kernel/vfs/generic.h"
#include "kernel-md/thread.h"

typedef void (*kthread_func_t)(void*);
struct VM_SPACE;
struct STACKFRAME;
struct Process;
class Result;

#define THREAD_MAX_NAME_LEN 32
#define THREAD_EVENT_EXIT 1

struct ThreadWaiter;
typedef util::List<ThreadWaiter> ThreadWaiterList;

struct Thread : public util::List<Thread>::NodePtr {
	void Ref();
	void Deref();
	void SetName(const char* name);

	void Suspend();
	void Resume();

	void SignalWaiters();
	void Wait();

	/* Machine-dependant data - must be first */
	MD_THREAD_FIELDS

	Spinlock t_lock;	/* Lock protecting the thread data */
	char t_name[THREAD_MAX_NAME_LEN + 1];	/* Thread name */

	refcount_t t_refcount;		/* Reference count of the thread, >0 */

	unsigned int t_flags;
#define THREAD_FLAG_ACTIVE	0x0001	/* Thread is scheduled somewhere */
#define THREAD_FLAG_SUSPENDED	0x0002	/* Thread is currently suspended */
#define THREAD_FLAG_ZOMBIE	0x0004	/* Thread has no more resources */
#define THREAD_FLAG_RESCHEDULE	0x0008	/* Thread desires a reschedule */
#define THREAD_FLAG_REAPING	0x0010	/* Thread will be reaped (destroyed by idle thread) */
#define THREAD_FLAG_MALLOC	0x0020	/* Thread is kmalloc()'ed */
#define THREAD_FLAG_TIMEOUT	0x0040	/* Timeout field is valid */
#define THREAD_FLAG_SIGPENDING	0x0080	/* Signal is pending */
#define THREAD_FLAG_KTHREAD	0x8000	/* Kernel thread */

	struct STACKFRAME* t_frame;
	unsigned int t_md_flags;

	unsigned int t_terminate_info;
#define THREAD_MAKE_EXITCODE(a,b) (((a) << 24) | ((b) & 0x00ffffff))
#define THREAD_TERM_SYSCALL	0	/* euthanasia */
#define THREAD_TERM_SIGNAL 1	/* terminated by signal */

	Process*		t_process;	/* associated process */

	int t_priority;			/* priority (0 highest) */
#define THREAD_PRIORITY_DEFAULT	200
#define THREAD_PRIORITY_IDLE	255
	int t_affinity;			/* thread CPU */
#define THREAD_AFFINITY_ANY -1

	/* Waiters to signal on thread changes */
	ThreadWaiterList	t_waitqueue;

	/* Timeout, when it expires the thread will be scheduled in */
	tick_t			t_timeout;

	/* Scheduler specific information */
	SchedulerPriv		t_sched_priv;

	signal::ThreadSpecificData t_sigdata;

	bool IsActive() const
	{
		return (t_flags & THREAD_FLAG_ACTIVE) != 0;
	}

	bool IsSuspended() const
	{
		return (t_flags & THREAD_FLAG_SUSPENDED) != 0;
	}

	bool IsZombie() const
	{
		return (t_flags & THREAD_FLAG_ZOMBIE) != 0;
	}

	bool IsRescheduling() const
	{
		return (t_flags & THREAD_FLAG_RESCHEDULE) != 0;
	}

	bool IsKernel() const
	{
		return (t_flags & THREAD_FLAG_KTHREAD) != 0;
	}

};

typedef util::List<Thread> ThreadList;

Result kthread_init(Thread& t, const char* name, kthread_func_t func, void* arg);

#define THREAD_ALLOC_DEFAULT	0	/* Nothing special */
#define THREAD_ALLOC_CLONE	1	/* Thread is created for cloning */

Result thread_alloc(Process& p, Thread*& dest, const char* name, int flags);

void idle_thread(void*);

void thread_sleep_ms(unsigned int ms);
void thread_exit(int exitcode);
void thread_dump(int num_args, char** arg);
Result thread_clone(Process& proc, Thread*& dest);

#endif
