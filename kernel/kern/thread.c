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
#include <machine/param.h>
#include <ananas/console.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/kdb.h>
#include <ananas/kmem.h>
#include <ananas/handle.h>
#include <ananas/pcpu.h>
#include <ananas/process.h>
#include <ananas/procinfo.h>
#include <ananas/reaper.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/thread.h>
#include <ananas/vm.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/vmspace.h>
#include "options.h"

TRACE_SETUP;

static spinlock_t spl_threadqueue = SPINLOCK_DEFAULT_INIT;
static struct THREAD_QUEUE thread_queue;

errorcode_t
thread_alloc(process_t* p, thread_t** dest, const char* name, int flags)
{
	/* First off, allocate the thread itself */
	thread_t* t = kmalloc(sizeof(struct THREAD));
	memset(t, 0, sizeof(struct THREAD));
	process_ref(p);
	t->t_process = p;
	t->t_flags = THREAD_FLAG_MALLOC;
	t->t_refcount = 1; /* caller */
	thread_set_name(t, name);

	/* Set up CPU affinity and priority */
	t->t_priority = THREAD_PRIORITY_DEFAULT;
	t->t_affinity = THREAD_AFFINITY_ANY;

	/* Ask machine-dependant bits to initialize our thread data */
	md_thread_init(t, flags);
	md_thread_set_argument(t, p->p_info_va);

	/* If we don't yet have a main thread, this thread will become the main */
	if (p->p_mainthread == NULL)
		p->p_mainthread = t;

	/* Initialize scheduler-specific parts */
	scheduler_init_thread(t);

	/* Add the thread to the thread queue */
	spinlock_lock(&spl_threadqueue);
	LIST_APPEND(&thread_queue, t);
	spinlock_unlock(&spl_threadqueue);

	*dest = t;
	return ANANAS_ERROR_OK;
}

errorcode_t
kthread_init(thread_t* t, const char* name, kthread_func_t func, void* arg)
{
	/*
	 * Kernel threads do not have an associated process, and thus no handles,
	 * vmspace and the like.
	 */
	memset(t, 0, sizeof(struct THREAD));
	t->t_flags = THREAD_FLAG_KTHREAD;
	t->t_refcount = 1;
	t->t_priority = THREAD_PRIORITY_DEFAULT;
	t->t_affinity = THREAD_AFFINITY_ANY;
	thread_set_name(t, name);

	/* Initialize MD-specifics */
	md_kthread_init(t, func, arg);

	/* Initialize scheduler-specific parts */
	scheduler_init_thread(t);

	/* Add the thread to the thread queue */
	spinlock_lock(&spl_threadqueue);
	LIST_APPEND(&thread_queue, t);
	spinlock_unlock(&spl_threadqueue);
	return ANANAS_ERROR_OK;
}

/*
 * thread_cleanup() migrates a thread to zombie-state; this is generally just
 * informing everyone about the thread's demise.
 */
static void
thread_cleanup(thread_t* t)
{
	struct PROCESS* p = t->t_process;
	KASSERT((t->t_flags & THREAD_FLAG_ZOMBIE) == 0, "cleaning up zombie thread %p", t);
	KASSERT((t->t_flags & THREAD_FLAG_KTHREAD) != 0 || p != NULL, "thread without process");

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
thread_destroy(thread_t* t)
{
	KASSERT(PCPU_GET(curthread) != t, "thread_destroy() on current thread");
	KASSERT(THREAD_IS_ZOMBIE(t), "thread_destroy() on a non-zombie thread");

	/* Free the machine-dependant bits */
	md_thread_free(t);

	/* Unreference the associated process */
	if (t->t_process != NULL)
		process_deref(t->t_process);
	t->t_process = NULL;

	/* If we aren't reaping the thread, remove it from our thread queue; it'll be gone soon */
	if ((t->t_flags & THREAD_FLAG_REAPING) == 0) {
		spinlock_lock(&spl_threadqueue);
		LIST_REMOVE(&thread_queue, t);
		spinlock_unlock(&spl_threadqueue);
	}

	if (t->t_flags & THREAD_FLAG_MALLOC)
		kfree(t);
	else
		memset(t, 0, sizeof(*t));
}

void
thread_ref(thread_t* t)
{
	KASSERT(t->t_refcount > 0, "reffing thread with invalid refcount %d", t->t_refcount);
	++t->t_refcount;
}

void
thread_deref(thread_t* t)
{
	KASSERT(t->t_refcount > 0, "dereffing thread with invalid refcount %d", t->t_refcount);

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
	int is_self = PCPU_GET(curthread) == t;
	if (is_self && !THREAD_IS_ZOMBIE(t)) {
		/* (1) - we can free most associated thread resources */
		thread_cleanup(t);
	}
	if (--t->t_refcount > 0) {
		/* (3) - refcount isn't yet zero so nothing to destroy */
		return;
	}

	if (is_self) {
		/*
		 * (2a) - mark the thread as reaping, and increment the refcount
		 *        so that we can just thread_deref() it
		 */
		t->t_flags |= THREAD_FLAG_REAPING;
		t->t_refcount++;

		/* Assign the thread to the reaper queue */
		spinlock_lock(&spl_threadqueue);
		LIST_REMOVE(&thread_queue, t);
		spinlock_unlock(&spl_threadqueue);
		reaper_enqueue(t);
		return;
	}

	/* (2b) Final ref - let's get rid of it */
	if (!THREAD_IS_ZOMBIE(t))
		thread_cleanup(t);
	thread_destroy(t);
}

void
thread_suspend(thread_t* t)
{
	TRACE(THREAD, FUNC, "t=%p", t);
	KASSERT(!THREAD_IS_SUSPENDED(t), "suspending suspended thread %p", t);
	KASSERT(t != PCPU_GET(idlethread), "suspending idle thread");
	scheduler_remove_thread(t);
}

void
thread_resume(thread_t* t)
{
	TRACE(THREAD, FUNC, "t=%p", t);

	/*
	 * In early startup (when we're not actually scheduling things), the idle
	 * thread is running which takes care of actually getting things initialized
	 * and ready - however, it will silently ignore all suspend actions which
	 * we need to catch here.
	 */
	if (!THREAD_IS_SUSPENDED(t)) {
		KASSERT(!scheduler_activated(), "resuming nonsuspended thread %p", t);
		return;
	}
	scheduler_add_thread(t);
}
	
void
thread_exit(int exitcode)
{
	thread_t* thread = PCPU_GET(curthread);
	TRACE(THREAD, FUNC, "t=%p, exitcode=%u", thread, exitcode);
	KASSERT(thread != NULL, "thread_exit() without thread");
	KASSERT(!THREAD_IS_ZOMBIE(thread), "exiting zombie thread");

	/* Store the result code; thread_free() will mark the thread as terminating */
	thread->t_terminate_info = exitcode;

	/* If we are the process' main thread, mark it as exiting */
	if (thread->t_process != NULL && thread->t_process->p_mainthread == thread)
		process_exit(thread->t_process, exitcode);

	/*
	 * Dereference our own thread handle; this will cause a transition to
	 * zombie-state - we are invoking case (1a) of thread_deref() here.
	 */
	thread_deref(thread);

	/* Ask the scheduler to exit the thread */
	scheduler_exit_thread(thread);
	/* NOTREACHED */
}

void
thread_set_name(thread_t* t, const char* name)
{
	if (t->t_flags & THREAD_FLAG_KTHREAD) {
		/* Surround kernel thread names by [ ] to clearly identify them */
		snprintf(t->t_name, THREAD_MAX_NAME_LEN, "[%s]", name);
	} else {
		strncpy(t->t_name, name, THREAD_MAX_NAME_LEN);
	}
	t->t_name[THREAD_MAX_NAME_LEN] = '\0';
}

errorcode_t
thread_clone(process_t* proc, thread_t** out_thread)
{
	TRACE(THREAD, FUNC, "proc=%p", proc);
	thread_t* curthread = PCPU_GET(curthread);

	struct THREAD* t;
	errorcode_t err = thread_alloc(proc, &t, curthread->t_name, THREAD_ALLOC_CLONE);
	ANANAS_ERROR_RETURN(err);

	/*
	 * Must copy the thread state over; note that this is the
	 * result of a system call, so we want to influence the
	 * return value.
	 */
	md_thread_clone(t, curthread, ANANAS_ERROR(CLONED));

	/* Thread is ready to rock */
	*out_thread = t;
	return ANANAS_ERROR_OK;
}

void
thread_signal_waiters(thread_t* t)
{
	spinlock_lock(&t->t_lock);
	while (!LIST_EMPTY(&t->t_waitqueue)) {
		struct THREAD_WAITER* tw = LIST_HEAD(&t->t_waitqueue);
		LIST_POP_HEAD(&t->t_waitqueue);
		sem_signal(&tw->tw_sem);
	}
	spinlock_unlock(&t->t_lock);
}

void
thread_wait(thread_t* t)
{
	struct THREAD_WAITER tw;
	sem_init(&tw.tw_sem, 0);

	spinlock_lock(&t->t_lock);
	LIST_APPEND(&t->t_waitqueue, &tw);
	spinlock_unlock(&t->t_lock);

	sem_wait(&tw.tw_sem);
	/* 'tw' will be removed by thread_signal_waiters() */
}

void
idle_thread()
{
	while(1) {
		md_cpu_relax();
	}
}

#if 0
extern struct THREAD* kdb_curthread;

KDB_COMMAND(threads, "[s:flags]", "Displays current threads")
{
	int flags = 0;

#define FLAG_HANDLE 1

	/* we use arguments as a mask to determine which information is to be dumped */
	for (int i = 1; i < num_args; i++) {
		for (const char* ptr = arg[i].a_u.u_string; *ptr != '\0'; ptr++)
			switch(*ptr) {
				case 'h': flags |= FLAG_HANDLE; break;
				default:
					kprintf("unknown modifier '%c', ignored\n", *ptr);
					break;
			}
	}

	struct THREAD* cur = PCPU_CURTHREAD();
	spinlock_lock(&spl_threadqueue);
	kprintf("thread dump\n");
	LIST_FOREACH(&thread_queue, t, struct THREAD) {
		kprintf ("thread %p (hindex %d): %s: flags [", t, t->t_hidx_thread, t->t_threadinfo->ti_args);
		if (THREAD_IS_ACTIVE(t))      kprintf(" active");
		if (THREAD_IS_SUSPENDED(t))   kprintf(" suspended");
		if (THREAD_IS_ZOMBIE(t))      kprintf(" zombie");
		kprintf(" ]%s\n", (t == cur) ? " <- current" : "");
		if (flags & FLAG_HANDLE) {
			kprintf("handles\n");
			for (unsigned int n = 0; n < THREAD_MAX_HANDLES; n++) {
				if (t->t_handle[n] == NULL)
					continue;
				kprintf(" %d: handle %p, type %u\n", n, t->t_handle[n], t->t_handle[n]->h_type);
			}
		}
	}
	spinlock_unlock(&spl_threadqueue);
}

KDB_COMMAND(thread, NULL, "Shows current thread information")
{
	if (kdb_curthread == NULL) {
		kprintf("no current thread set\n");
		return;
	}

	struct THREAD* thread = kdb_curthread;
	kprintf("arg          : '%s'\n", thread->t_threadinfo->ti_args);
	kprintf("flags        : 0x%x\n", thread->t_flags);
	kprintf("terminateinfo: 0x%x\n", thread->t_terminate_info);
	kprintf("mappings:\n");
	LIST_FOREACH(&thread->t_vmspace->vs_areas, va, vmarea_t) {
		kprintf("   flags      : 0x%x\n", va->va_flags);
		kprintf("   virtual    : 0x%x - 0x%x\n", va->va_virt, va->va_virt + va->va_len);
		kprintf("   length     : %u\n", va->va_len);
		kprintf("\n");
	}
}

KDB_COMMAND(bt, NULL, "Current thread backtrace")
{
	if (kdb_curthread == NULL) {
		kprintf("no current thread set\n");
		return;
	}

#ifdef __amd64__
	struct THREAD* thread = kdb_curthread;
	for (int x = 0; x <= 32; x += 8)
		kprintf("rbp %d -> %p\n", x, *(register_t*)(thread->md_rsp + x));

	register_t rbp = *(register_t*)(thread->md_rsp + 24);
	kprintf("rbp %p rsp %p\n", rbp, thread->md_rsp);
  while(rbp >= 0xffff880000000000) {
    kprintf("[%p] ", *(uint64_t*)(rbp + 8));
    rbp = *(uint64_t*)rbp;
  }
#endif
}

KDB_COMMAND(curthread, "i:thread", "Sets current thread")
{
	kdb_curthread = (struct THREAD*)arg[1].a_u.u_value;
}
#endif /* KDB */

/* vim:set ts=2 sw=2: */
