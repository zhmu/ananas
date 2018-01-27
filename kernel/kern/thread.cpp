/*
 * Threads have a current state which is contained in t_flags; possible
 * transitions are:
 *
 *  +-->[suspended]->-+
 *  |       |         |
 *  |       v         |
 *  +-<--[active]     |
 *          |         |
 *          v         |
 *       [zombie]<----+
 *          |
 *          v
 *       [(gone)]
 *
 * All transitions are managed by scheduler.c.
 */
#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/procinfo.h>
#include "kernel/device.h"
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/pcpu.h"
#include "kernel/process.h"
#include "kernel/reaper.h"
#include "kernel/schedule.h"
#include "kernel/time.h"
#include "kernel/trace.h"
#include "kernel/thread.h"
#include "kernel/vm.h"
#include "options.h"

TRACE_SETUP;

static Spinlock spl_threadqueue;
static ThreadList allThreads;

errorcode_t
thread_alloc(Process& p, Thread*& dest, const char* name, int flags)
{
	/* First off, allocate the thread itself */
	auto t = new Thread;
	memset(t, 0, sizeof(Thread));
	process_ref(p);
	t->t_process = &p;
	t->t_flags = THREAD_FLAG_MALLOC;
	t->t_refcount = 1; /* caller */
	thread_set_name(*t, name);

	/* Set up CPU affinity and priority */
	t->t_priority = THREAD_PRIORITY_DEFAULT;
	t->t_affinity = THREAD_AFFINITY_ANY;

	/* Ask machine-dependant bits to initialize our thread data */
	md_thread_init(*t, flags);
	md_thread_set_argument(*t, p.p_info_va);

	/* If we don't yet have a main thread, this thread will become the main */
	if (p.p_mainthread == NULL)
		p.p_mainthread = t;

	/* Initialize scheduler-specific parts */
	scheduler_init_thread(*t);

	/* Add the thread to the thread queue */
	spinlock_lock(spl_threadqueue);
	allThreads.push_back(*t);
	spinlock_unlock(spl_threadqueue);

	dest = t;
	return ananas_success();
}

errorcode_t
kthread_init(Thread& t, const char* name, kthread_func_t func, void* arg)
{
	/*
	 * Kernel threads do not have an associated process, and thus no handles,
	 * vmspace and the like.
	 */
	memset(&t, 0, sizeof(Thread));
	t.t_flags = THREAD_FLAG_KTHREAD;
	t.t_refcount = 1;
	t.t_priority = THREAD_PRIORITY_DEFAULT;
	t.t_affinity = THREAD_AFFINITY_ANY;
	thread_set_name(t, name);

	/* Initialize MD-specifics */
	md_kthread_init(t, func, arg);

	/* Initialize scheduler-specific parts */
	scheduler_init_thread(t);

	/* Add the thread to the thread queue */
	spinlock_lock(spl_threadqueue);
	allThreads.push_back(t);
	spinlock_unlock(spl_threadqueue);
	return ananas_success();
}

/*
 * thread_cleanup() migrates a thread to zombie-state; this is generally just
 * informing everyone about the thread's demise.
 */
static void
thread_cleanup(Thread& t)
{
	Process* p = t.t_process;
	KASSERT((t.t_flags & THREAD_FLAG_ZOMBIE) == 0, "cleaning up zombie thread %p", &t);
	KASSERT((t.t_flags & THREAD_FLAG_KTHREAD) != 0 || p != nullptr, "thread without process");

	/*
	 * Signal anyone waiting on the thread; the terminate information should
	 * already be set at this point - note that handle_wait() will do additional
	 * checks to ensure the thread is truly gone.
	 */
	thread_signal_waiters(t);
}

/*
 * thread_destroy() take a zombie thread and completely frees it (the thread
 * will not be valid after calling this function and thus this can only be
 * called from a different thread)
 */
static void
thread_destroy(Thread& t)
{
	KASSERT(PCPU_GET(curthread) != &t, "thread_destroy() on current thread");
	KASSERT(THREAD_IS_ZOMBIE(&t), "thread_destroy() on a non-zombie thread");

	/* Free the machine-dependant bits */
	md_thread_free(t);

	/* Unreference the associated process */
	if (t.t_process != nullptr)
		process_deref(*t.t_process);
	t.t_process = nullptr;

	/* If we aren't reaping the thread, remove it from our thread queue; it'll be gone soon */
	if ((t.t_flags & THREAD_FLAG_REAPING) == 0) {
		spinlock_lock(spl_threadqueue);
		allThreads.remove(t);
		spinlock_unlock(spl_threadqueue);
	}

	if (t.t_flags & THREAD_FLAG_MALLOC)
		delete &t;
	else
		memset(&t, 0, sizeof(Thread));
}

void
thread_ref(Thread& t)
{
	KASSERT(t.t_refcount > 0, "reffing thread with invalid refcount %d", t.t_refcount);
	++t.t_refcount;
}

void
thread_deref(Thread& t)
{
	KASSERT(t.t_refcount > 0, "dereffing thread with invalid refcount %d", t.t_refcount);

	/*
	 * Thread cleanup is quite involved - we need to take the following into account:
	 *
	 * 1) A thread itself calling thread_deref() can demote the thread to zombie-state
	 *    This is because the owner decides to stop the thread, which means we can free
	 *    most associated resources.
	 * 2) The new refcount would become 0
	 *    We can go to zombie-state if we aren't already. However:
	 *    2a) If curthread == 't', we can't destroy because we are using the
	 *        threads stack. We'll postpone destruction to another thread.
	 *    2b) Otherwise, we can destroy 't' and be done.
	 * 3) Otherwise, the refcount > 0 and we must let the thread live.
	 */
	int is_self = PCPU_GET(curthread) == &t;
	if (is_self && !THREAD_IS_ZOMBIE(&t)) {
		/* (1) - we can free most associated thread resources */
		thread_cleanup(t);
	}
	if (--t.t_refcount > 0) {
		/* (3) - refcount isn't yet zero so nothing to destroy */
		return;
	}

	if (is_self) {
		/*
		 * (2a) - mark the thread as reaping, and increment the refcount
		 *        so that we can just thread_deref() it
		 */
		t.t_flags |= THREAD_FLAG_REAPING;
		t.t_refcount++;

		/* Assign the thread to the reaper queue */
		spinlock_lock(spl_threadqueue);
		allThreads.remove(t);
		spinlock_unlock(spl_threadqueue);
		reaper_enqueue(t);
		return;
	}

	/* (2b) Final ref - let's get rid of it */
	if (!THREAD_IS_ZOMBIE(&t))
		thread_cleanup(t);
	thread_destroy(t);
}

void
thread_suspend(Thread& t)
{
	TRACE(THREAD, FUNC, "t=%p", &t);
	KASSERT(!THREAD_IS_SUSPENDED(&t), "suspending suspended thread %p", &t);
	KASSERT(&t != PCPU_GET(idlethread), "suspending idle thread");
	scheduler_remove_thread(t);
}

void
thread_sleep(tick_t num_ticks)
{
	Thread* t = PCPU_GET(curthread);
	t->t_timeout = Ananas::Time::GetTicks() + num_ticks;
	t->t_flags |= THREAD_FLAG_TIMEOUT;
	thread_suspend(*t);
	schedule();
}

void
thread_resume(Thread& t)
{
	TRACE(THREAD, FUNC, "t=%p", &t);

	/*
	 * In early startup (when we're not actually scheduling things), the idle
	 * thread is running which takes care of actually getting things initialized
	 * and ready - however, it will silently ignore all suspend actions which
	 * we need to catch here.
	 */
	if (!THREAD_IS_SUSPENDED(&t)) {
		KASSERT(!scheduler_activated(), "resuming nonsuspended thread %p", &t);
		return;
	}
	scheduler_add_thread(t);
}

void
thread_exit(int exitcode)
{
	Thread* thread = PCPU_GET(curthread);
	TRACE(THREAD, FUNC, "t=%p, exitcode=%u", thread, exitcode);
	KASSERT(thread != NULL, "thread_exit() without thread");
	KASSERT(!THREAD_IS_ZOMBIE(thread), "exiting zombie thread");

	/* Store the result code; thread_free() will mark the thread as terminating */
	thread->t_terminate_info = exitcode;

	/* If we are the process' main thread, mark it as exiting */
	if (thread->t_process != nullptr && thread->t_process->p_mainthread == thread)
		process_exit(*thread->t_process, exitcode);

	/*
	 * Dereference our own thread handle; this will cause a transition to
	 * zombie-state - we are invoking case (1a) of thread_deref() here.
	 */
	thread_deref(*thread);

	/* Ask the scheduler to exit the thread */
	scheduler_exit_thread(*thread);
	/* NOTREACHED */
}

void
thread_set_name(Thread& t, const char* name)
{
	if (t.t_flags & THREAD_FLAG_KTHREAD) {
		/* Surround kernel thread names by [ ] to clearly identify them */
		snprintf(t.t_name, THREAD_MAX_NAME_LEN, "[%s]", name);
	} else {
		strncpy(t.t_name, name, THREAD_MAX_NAME_LEN);
	}
	t.t_name[THREAD_MAX_NAME_LEN] = '\0';
}

errorcode_t
thread_clone(Process& proc, Thread*& out_thread)
{
	TRACE(THREAD, FUNC, "proc=%p", &proc);
	Thread* curthread = PCPU_GET(curthread);

	Thread* t;
	errorcode_t err = thread_alloc(proc, t, curthread->t_name, THREAD_ALLOC_CLONE);
	ANANAS_ERROR_RETURN(err);

	/*
	 * Must copy the thread state over; note that this is the
	 * result of a system call, so we want to influence the
	 * return value.
	 */
	md_thread_clone(*t, *curthread, ANANAS_ERROR(CLONED));

	/* Thread is ready to rock */
	out_thread = t;
	return ananas_success();
}

void
thread_signal_waiters(Thread& t)
{
	spinlock_lock(t.t_lock);
	while(!t.t_waitqueue.empty()) {
		auto& tw = t.t_waitqueue.front();
		t.t_waitqueue.pop_front();

		sem_signal(tw.tw_sem);
	}
	spinlock_unlock(t.t_lock);
}

void
thread_wait(Thread& t)
{
	ThreadWaiter tw;
	sem_init(tw.tw_sem, 0);

	spinlock_lock(t.t_lock);
	t.t_waitqueue.push_back(tw);
	spinlock_unlock(t.t_lock);

	sem_wait(tw.tw_sem);
	/* 'tw' will be removed by thread_signal_waiters() */
}

void
idle_thread(void*)
{
	while(1) {
		md_cpu_relax();
	}
}

/* vim:set ts=2 sw=2: */
