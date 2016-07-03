#include <ananas/types.h>
#include <machine/param.h>
#include <ananas/console.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/kdb.h>
#include <ananas/kmem.h>
#include <ananas/handle.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/thread.h>
#include <ananas/threadinfo.h>
#include <ananas/vm.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/vmspace.h>
#include "options.h"

TRACE_SETUP;

static spinlock_t spl_threadqueue = SPINLOCK_DEFAULT_INIT;
static spinlock_t spl_threadfuncs = SPINLOCK_DEFAULT_INIT;
static struct THREAD_QUEUE threadqueue;
static struct THREAD_CALLBACKS threadcallbacks_init;
static struct THREAD_CALLBACKS threadcallbacks_exit;

static errorcode_t
thread_init(thread_t* t, thread_t* parent, int flags)
{
	errorcode_t err;

	struct HANDLE* thread_handle;

	memset(t, 0, sizeof(struct THREAD));
	err = handle_alloc(HANDLE_TYPE_THREAD, t /* XXX should be parent? */, THREAD_MAX_HANDLES - 1, &thread_handle, &t->t_hidx_thread);
	ANANAS_ERROR_RETURN(err);
	thread_handle->h_data.d_thread = t;
	t->t_refcount = 1; /* caller */

	/* Set up CPU affinity and priority */
	t->t_priority = THREAD_PRIORITY_DEFAULT;
	t->t_affinity = THREAD_AFFINITY_ANY;

	/* Create the thread's vmspace */
	err = vmspace_create(&t->t_vmspace);
	if (err != ANANAS_ERROR_NONE)
		goto fail;

	/* ask machine-dependant bits to initialize our thread data */
	md_thread_init(t, flags);

	/* Create thread information structure */
	t->t_threadinfo = page_alloc_length_mapped(sizeof(struct THREADINFO), &t->t_threadinfo_page, VM_FLAG_READ | VM_FLAG_WRITE);
	if (t->t_threadinfo == NULL)
		return ANANAS_ERROR(OUT_OF_MEMORY);

	/* Initialize thread information structure */
	memset(t->t_threadinfo, 0, sizeof(struct THREADINFO));
	t->t_threadinfo->ti_size = sizeof(struct THREADINFO);
	t->t_threadinfo->ti_handle = t->t_hidx_thread;
	if (parent != NULL)
		thread_set_environment(t, parent->t_threadinfo->ti_env, PAGE_SIZE /* XXX */);

	/* Map the thread information structure in thread-space */
	vmarea_t* va;
	err = vmspace_map(t->t_vmspace, page_get_paddr(t->t_threadinfo_page), sizeof(struct THREADINFO), VM_FLAG_USER | VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_PRIVATE, &va);
	if (err != ANANAS_ERROR_NONE)
		goto fail;
	md_thread_set_argument(t, va->va_virt);

	/* Clone the parent's handles - we skip the thread handle */
	if (parent != NULL) {
		for (unsigned int n = 0; n < THREAD_MAX_HANDLES; n++) {
			if (n == parent->t_hidx_thread)
				continue; /* do not clone the parent handle */
			if (parent->t_handle[n] == NULL)
				continue;

			struct HANDLE* handle;
			handleindex_t out;
			err = handle_clone(parent, n, NULL, t, &handle, n, &out);
			ANANAS_ERROR_RETURN(err); /* XXX clean up */
			KASSERT(n == out, "cloned handle %d to new handle %d", n, out);
		}
	}

	/* Initialize scheduler-specific parts */
	scheduler_init_thread(t);

	/* Run all thread initialization callbacks */
	if (!DQUEUE_EMPTY(&threadcallbacks_init))
		DQUEUE_FOREACH(&threadcallbacks_init, tc, struct THREAD_CALLBACK) {
			tc->tc_func(t, parent);
		}

	/* Add the thread to the thread queue */
	spinlock_lock(&spl_threadqueue);
	DQUEUE_ADD_TAIL(&threadqueue, t);
	spinlock_unlock(&spl_threadqueue);
	return ANANAS_ERROR_OK;

fail:
	/* XXXLEAK cleanup */
	kprintf("thread_init(): XXX deal with error case cleanups (err=%i)\n", err);
	return err;
}

errorcode_t
kthread_init(thread_t* t, kthread_func_t func, void* arg)
{
	errorcode_t err;

	/* Create a basic thread; we'll only make a thread handle, no I/O */
	struct HANDLE* thread_handle;
	memset(t, 0, sizeof(struct THREAD));
	t->t_flags = THREAD_FLAG_KTHREAD;
	err = handle_alloc(HANDLE_TYPE_THREAD, t, 0, &thread_handle, &t->t_hidx_thread);
	ANANAS_ERROR_RETURN(err);
	thread_handle->h_data.d_thread = t;

	/* Set up CPU affinity and priority */
	t->t_priority = THREAD_PRIORITY_DEFAULT;
	t->t_affinity = THREAD_AFFINITY_ANY;

	/* Initialize dummy threadinfo; this is used to store the thread name */
	t->t_threadinfo = page_alloc_length_mapped(sizeof(struct THREADINFO), &t->t_threadinfo_page, VM_FLAG_READ | VM_FLAG_WRITE);
	if (t->t_threadinfo == NULL)
		return ANANAS_ERROR(OUT_OF_MEMORY);

	/* Initialize MD-specifics */
	md_kthread_init(t, func, arg);

	/* Initialize scheduler-specific parts */
	scheduler_init_thread(t);

	/* Run all thread initialization callbacks */
	if (!DQUEUE_EMPTY(&threadcallbacks_init))
		DQUEUE_FOREACH(&threadcallbacks_init, tc, struct THREAD_CALLBACK) {
			tc->tc_func(t, NULL /* kthreads have no parent */);
		}

	/* Add the thread to the thread queue */
	spinlock_lock(&spl_threadqueue);
	DQUEUE_ADD_TAIL(&threadqueue, t);
	spinlock_unlock(&spl_threadqueue);
	return ANANAS_ERROR_OK;
}

errorcode_t
thread_alloc(thread_t* parent, thread_t** dest, int flags)
{
	thread_t* t = kmalloc(sizeof(struct THREAD));
	errorcode_t err = thread_init(t, parent, flags);
	if (err == ANANAS_ERROR_NONE)
		*dest = t;
	return err;
}

/*
 * thread_cleanup() migrates a thread to zombie-state; this involves freeing
 * most resources.
 */
static void
thread_cleanup(thread_t* t)
{
	KASSERT((t->t_flags & THREAD_FLAG_ZOMBIE) == 0, "freeing zombie thread %p", t);

	errorcode_t err;
	struct HANDLE* thread_handle;
	err = handle_lookup(t, t->t_hidx_thread, HANDLE_TYPE_THREAD, &thread_handle);
	KASSERT(err == ANANAS_ERROR_NONE, "cannot look up thread handle for %p: %d", t, err);

	/*
	 * Free all handles in use by the thread. Note that we must not free the thread
	 * handle itself, as the thread isn't destroyed yet (we are just freeing all
	 * resources here - the thread handle itself is necessary for obtaining the
	 * result code etc)
	 *
	 * XXX LOCK
	 */
	for(unsigned int n = 0; n < THREAD_MAX_HANDLES; n++) {
		if (n != t->t_hidx_thread)
			handle_free_byindex(t, n);
	}

	/* Clean the thread's vmspace up - this will remove all non-essential mappings */
	vmspace_cleanup(t->t_vmspace);

	/*
	 * Clear the thread information; no one can query it at this point as the
	 * thread itself will not run anymore.
	 */
	page_free(t->t_threadinfo_page);
	t->t_threadinfo = NULL;

	/* Run all thread exit callbacks */
	if (!DQUEUE_EMPTY(&threadcallbacks_exit))
		DQUEUE_FOREACH(&threadcallbacks_exit, tc, struct THREAD_CALLBACK) {
			tc->tc_func(t, NULL);
		}

	/*
	 * Signal anyone waiting on the thread; the terminate information should
	 * already be set at this point - note that handle_wait() will do additional
	 * checks to ensure the thread is truly gone.
	 */
	thread_signal_waiters(t);

	/*
	 * Force our thread handle to point to nothing; we're about to free it, but we
	 * don't want it to deref us again.
	 */
	thread_handle->h_data.d_thread = NULL;

	/*
	 * Throw away the thread handle itself; it will be removed once it runs out of
	 * references (which will be the case once everyone is done waiting for it
	 * and cleans up its own handle)
	 */
	handle_free_byindex(t, t->t_hidx_thread);
}

/*
 * thread_destroy() take a zombie thread and completely frees it (the thread
 * will not be valid after calling this function)
 */
static void
thread_destroy(thread_t* t)
{
	KASSERT(PCPU_GET(curthread) != t, "thread_destroy() on current thread");
	KASSERT(THREAD_IS_ZOMBIE(t), "thread_destroy() on a non-zombie thread");
	KASSERT((t), "thread_destroy() on a non-zombie thread");

	/* Free the machine-dependant bits */
	md_thread_free(t);

	/* Get rid of the thread's vmspace - this removes all MD-dependant bits too */
	vmspace_destroy(t->t_vmspace);
	t->t_vmspace = NULL;

	/* Remove the thread from our thread queue; it'll be gone soon */
	spinlock_lock(&spl_threadqueue);
	DQUEUE_REMOVE(&threadqueue, t);
	spinlock_unlock(&spl_threadqueue);

	kfree(t);
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
	 * Thread cleanup is quite involved - we have two scenario's:
	 *
	 *  1) t is in non-zombie state
	 *  2) t is in zombie-state
	 *
	 * For each of which there are two cases:
	 *
	 *  a) Thread itself is calling thread_deref()
	 *  b) Other thread is calling thread_deref()
	 *
	 * Where we need to do the following (each of which involves decrementing
	 * the refcount):
	 *
	 * (1a) It is safe to move the thread to zombie-state as the thread itself
	 *      requested the free.
 	 * (1b) Nothing special; if the refcount reaches zero we must move the
 	 *      thread to zombie-state and move to (2b) - note that this
	 *      shouldn't ordinarily happen: where did the thread's ref to itself
	 *      go?
	 * (2a) Nothing - we can't clean up our own resources any futher. If we took
	 *      the last ref, we must give one to another thread because we cannot
	 *      clean ourselves up.
	 * (2b) Clean up the thread if this is the final ref.
	 */
	int is_self = PCPU_GET(curthread) == t;
	if (!THREAD_IS_ZOMBIE(t)) /* (1) */ {
		if (is_self) /* (1a) */ {
			thread_cleanup(t);
		} else /* (1b) */ {
			if (t->t_refcount == 1) {
				kprintf("thread_deref(): warning: 1b with refcount 1!\n");
			}
		}
	} else /* (2) */ {
		if (is_self) /* (2a) */ {
			panic("(2a), t=%p", t);
		}
	}

	if (--t->t_refcount > 0)
		return;

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

	/*
	 * Dereference our own thread handle; this will cause a transition to
	 * zombie-state - we are invoking case (1a) of thread_deref() here.
	 */
	thread_deref(thread);

	/* Ask the scheduler to exit the thread */
	scheduler_exit_thread(thread);
	/* NOTREACHED */
}

errorcode_t
thread_clone(struct THREAD* parent, int flags, struct THREAD** dest)
{
	TRACE(THREAD, FUNC, "parent=%p, flags=%u", parent, flags);
	errorcode_t err;

	KASSERT(PCPU_GET(curthread) == parent, "thread_clone(): unsupported from non-current thread (curthread %p != parent %p)", PCPU_GET(curthread), parent);

	struct THREAD* t;
	err = thread_alloc(parent, &t, THREAD_ALLOC_CLONE);
	ANANAS_ERROR_RETURN(err);

	/* Duplicate the vmspace */
	err = vmspace_clone(parent->t_vmspace, t->t_vmspace);
	ANANAS_ERROR_RETURN(err); /* XXX clean up t! */

	/*
	 * Copy the thread's arguments over; the environment will already been
	 * inherited if necessary.
	 */
	memcpy(t->t_threadinfo->ti_args, parent->t_threadinfo->ti_args, THREADINFO_ARGS_LENGTH);

	/*
	 * Must copy the thread state over; note that this is the
	 * result of a system call, so we want to influence the
	 * return value.
	 */
	md_thread_clone(t, parent, ANANAS_ERROR(CLONED));

	/* Thread is ready to rock */
	*dest = t;
	return ANANAS_ERROR_OK;
}

errorcode_t
thread_set_args(thread_t* t, const char* args, size_t args_len)
{
	for (unsigned int i = 0; i < ((THREADINFO_ARGS_LENGTH - 1) < args_len ? (THREADINFO_ARGS_LENGTH - 1) : args_len); i++)
		if(args[i] == '\0' && args[i + 1] == '\0') {
			memcpy(t->t_threadinfo->ti_args, args, i + 2 /* terminating \0\0 */);
			return ANANAS_ERROR_OK;
		}
	return ANANAS_ERROR(BAD_LENGTH);
}

errorcode_t
thread_set_environment(thread_t* t, const char* env, size_t env_len)
{
	for (unsigned int i = 0; i < ((THREADINFO_ENV_LENGTH - 1) < env_len ? (THREADINFO_ENV_LENGTH - 1) : env_len); i++)
		if(env[i] == '\0' && env[i + 1] == '\0') {
			memcpy(t->t_threadinfo->ti_env, env, i + 2 /* terminating \0\0 */);
			return ANANAS_ERROR_OK;
		}

	return ANANAS_ERROR(BAD_LENGTH);
}

errorcode_t
thread_register_init_func(struct THREAD_CALLBACK* fn)
{
	spinlock_lock(&spl_threadfuncs);
	DQUEUE_ADD_TAIL(&threadcallbacks_init, fn);
	spinlock_unlock(&spl_threadfuncs);	
	return ANANAS_ERROR_OK;
}

errorcode_t
thread_register_exit_func(struct THREAD_CALLBACK* fn)
{
	spinlock_lock(&spl_threadfuncs);
	DQUEUE_ADD_TAIL(&threadcallbacks_exit, fn);
	spinlock_unlock(&spl_threadfuncs);	
	return ANANAS_ERROR_OK;
}

errorcode_t
thread_unregister_init_func(struct THREAD_CALLBACK* fn)
{
	spinlock_lock(&spl_threadfuncs);
	DQUEUE_REMOVE(&threadcallbacks_init, fn);
	spinlock_unlock(&spl_threadfuncs);	
	return ANANAS_ERROR_OK;
}

errorcode_t
thread_unregister_exit_func(struct THREAD_CALLBACK* fn)
{
	spinlock_lock(&spl_threadfuncs);
	DQUEUE_REMOVE(&threadcallbacks_exit, fn);
	spinlock_unlock(&spl_threadfuncs);	
	return ANANAS_ERROR_OK;
}

void
thread_signal_waiters(thread_t* t)
{
	spinlock_lock(&t->t_lock);
	while (!DQUEUE_EMPTY(&t->t_waitqueue)) {
		struct THREAD_WAITER* tw = DQUEUE_HEAD(&t->t_waitqueue);
		DQUEUE_POP_HEAD(&t->t_waitqueue);
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
	DQUEUE_ADD_TAIL(&t->t_waitqueue, &tw);
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

#ifdef OPTION_KDB
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
	if (!DQUEUE_EMPTY(&threadqueue))
		DQUEUE_FOREACH(&threadqueue, t, struct THREAD) {
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
	if (!DQUEUE_EMPTY(&thread->t_vmspace->vs_areas)) {
		DQUEUE_FOREACH(&thread->t_vmspace->vs_areas, va, vmarea_t) {
			kprintf("   flags      : 0x%x\n", va->va_flags);
			kprintf("   virtual    : 0x%x - 0x%x\n", va->va_virt, va->va_virt + va->va_len);
			kprintf("   length     : %u\n", va->va_len);
			kprintf("\n");
		}
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
