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
#include <machine/vm.h> /* for aTOb; a,b in [PV] */
#include "options.h"

TRACE_SETUP;

static spinlock_t spl_threadqueue = SPINLOCK_DEFAULT_INIT;
static spinlock_t spl_threadfuncs = SPINLOCK_DEFAULT_INIT;
static struct THREAD_QUEUE threadqueue;
static struct THREAD_CALLBACKS threadcallbacks_init;
static struct THREAD_CALLBACKS threadcallbacks_exit;

errorcode_t
thread_init(thread_t* t, thread_t* parent, int flags)
{
	errorcode_t err;

	struct HANDLE* thread_handle;

	memset(t, 0, sizeof(struct THREAD));
	DQUEUE_INIT(&t->t_mappings);
	err = handle_alloc(HANDLE_TYPE_THREAD, t /* XXX should be parent? */, THREAD_MAX_HANDLES - 1, &thread_handle, &t->t_hidx_thread);
	ANANAS_ERROR_RETURN(err);
	thread_handle->h_data.d_thread = t;
	DQUEUE_INIT(&t->t_pages);

	/* Set up CPU affinity and priority */
	t->t_priority = THREAD_PRIORITY_DEFAULT;
	t->t_affinity = THREAD_AFFINITY_ANY;

	/* ask machine-dependant bits to initialize our thread data */
	md_thread_init(t, flags);

	/* Create thread information structure */
	struct PAGE* threadinfo_page;
	t->t_threadinfo = page_alloc_length_mapped(sizeof(struct THREADINFO), &threadinfo_page, VM_FLAG_READ | VM_FLAG_WRITE);
	if (t->t_threadinfo == NULL)
		return ANANAS_ERROR(OUT_OF_MEMORY);
	DQUEUE_ADD_TAIL(&t->t_pages, threadinfo_page);

	/* Initialize thread information structure */
	memset(t->t_threadinfo, 0, sizeof(t->t_threadinfo));
	t->t_threadinfo->ti_size = sizeof(struct THREADINFO);
	t->t_threadinfo->ti_handle = t->t_hidx_thread;
	if (parent != NULL)
		thread_set_environment(t, parent->t_threadinfo->ti_env, PAGE_SIZE /* XXX */);

	/* Map the thread information structure in thread-space */
	struct THREAD_MAPPING* tm;
	err = thread_map(t, page_get_paddr(threadinfo_page), sizeof(struct THREADINFO), THREAD_MAP_READ | THREAD_MAP_WRITE | THREAD_MAP_PRIVATE, &tm);
	if (err != ANANAS_ERROR_NONE)
		goto fail;
	md_thread_set_argument(t, tm->tm_virt);

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
	DQUEUE_INIT(&t->t_mappings);
	t->t_flags = THREAD_FLAG_KTHREAD;
	err = handle_alloc(HANDLE_TYPE_THREAD, t, 0, &thread_handle, &t->t_hidx_thread);
	ANANAS_ERROR_RETURN(err);
	thread_handle->h_data.d_thread = t;

	/* Set up CPU affinity and priority */
	t->t_priority = THREAD_PRIORITY_DEFAULT;
	t->t_affinity = THREAD_AFFINITY_ANY;

	/* Initialize dummy threadinfo; this is used to store the thread name */
	struct PAGE* threadinfo_page;
	t->t_threadinfo = page_alloc_length_mapped(sizeof(struct THREADINFO), &threadinfo_page, VM_FLAG_READ | VM_FLAG_WRITE);
	if (t->t_threadinfo == NULL)
		return ANANAS_ERROR(OUT_OF_MEMORY);
	DQUEUE_ADD_TAIL(&t->t_pages, threadinfo_page);

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

void
thread_free_mapping(thread_t* t, struct THREAD_MAPPING* tm)
{
	TRACE(THREAD, INFO, "thread_free_mapping(): t=%p tm=%p", t, tm);
	DQUEUE_REMOVE(&t->t_mappings, tm);
	if (tm->tm_destroy != NULL)
		tm->tm_destroy(t, tm);

	/* If the pages were allocated, we need to free them one by one */
	if (!DQUEUE_EMPTY(&tm->tm_pages))
		DQUEUE_FOREACH_SAFE(&tm->tm_pages, p, struct PAGE) {
			page_free(p);
		}
	kfree(tm);
}

void
thread_free_mappings(thread_t* t)
{
	/*
	 * Free all mapped process memory, if needed. We don't bother to unmap them
	 * in the thread's VM as it's going away anyway XXX we should for !x86
	 */
	while(!DQUEUE_EMPTY(&t->t_mappings)) {
		struct THREAD_MAPPING* tm = DQUEUE_HEAD(&t->t_mappings);
		thread_free_mapping(t, tm); /* also removed the mapping */
	}
}

void
thread_free(thread_t* t)
{
	KASSERT((t->t_flags & THREAD_FLAG_ZOMBIE) == 0, "freeing zombie thread %p", t);

	errorcode_t err;
	struct HANDLE* thread_handle;
	err = handle_lookup(t, t->t_hidx_thread, HANDLE_TYPE_THREAD, &thread_handle);
	KASSERT(err == ANANAS_ERROR_NONE, "cannot look up thread handle for %p: %d", t, err);

	KASSERT(PCPU_GET(curthread) != t || thread_handle->h_refcount > 1, "freeing current thread with invalid refcount %d", thread_handle->h_refcount);

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

	/*
	 * Free all thread mappings; this must be done afterwards because memory handles are
	 * implemented as mappings and they are already gone by now
	 */
	thread_free_mappings(t);

	/*
	 * Clear the thread information; no one can query it at this point as the
 	 * thread itself will not run anymore. The backing page will be removed
	 * in thread_destroy().
	 */
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
	handle_signal(thread_handle, THREAD_EVENT_EXIT, t->t_terminate_info);

	/*
	 * Throw away the thread handle itself; it will be removed once it runs out of
	 * references (which will be the case once everyone is done waiting for it
	 * and cleans up its own handle)
	 */
	handle_free_byindex(t, t->t_hidx_thread);
}

void
thread_destroy(thread_t* t)
{
	KASSERT(PCPU_GET(curthread) != t, "thread_destroy() on current thread");
	KASSERT(THREAD_IS_ZOMBIE(t), "thread_destroy() on a non-zombie thread");

	/* Free the machine-dependant bits */
	md_thread_free(t);

	/*
	 * Throw away all final mappings the thread may have; we do this here
	 * to allow things like a stack and pagetables to reside there, as only
	 * now it is safe to release them.
	 */
	if (!DQUEUE_EMPTY(&t->t_pages)) {
		DQUEUE_FOREACH_SAFE(&t->t_pages, p, struct PAGE) {
			page_free(p);
		}
	}

	/* Remove the thread from our thread queue; it'll be gone soon */
	spinlock_lock(&spl_threadqueue);
	DQUEUE_REMOVE(&threadqueue, t);
	spinlock_unlock(&spl_threadqueue);

	kfree(t);
}

static int
thread_make_vmflags(unsigned int flags)
{
	int vm_flags = 0;
	if (flags & THREAD_MAP_READ)
		vm_flags |= VM_FLAG_READ;
	if (flags & THREAD_MAP_WRITE)
		vm_flags |= VM_FLAG_WRITE;
	if (flags & THREAD_MAP_EXECUTE)
		vm_flags |= VM_FLAG_EXECUTE;
	if (flags & THREAD_MAP_DEVICE)
		vm_flags |= VM_FLAG_DEVICE;
	return vm_flags;
}

/*
 * Maps a piece of kernel memory 'from' to 'to' for thread 't'.
 * 'len' is the length in bytes; 'flags' contains mapping flags:
 *  THREAD_MAP_ALLOC - allocate a new piece of memory (will be zeroed first)
 * Returns the new mapping structure
 */
errorcode_t
thread_mapto(thread_t* t, addr_t virt, addr_t phys, size_t len, uint32_t flags, struct THREAD_MAPPING** out)
{
	/* Check whether a mapping already exists; if so, we must refuse */
	if (!DQUEUE_EMPTY(&t->t_mappings)) {
		DQUEUE_FOREACH(&t->t_mappings, tm, struct THREAD_MAPPING) {
			if ((virt >= tm->tm_virt && (virt + len) <= (tm->tm_virt + tm->tm_len)) ||
			    (tm->tm_virt >= virt && (tm->tm_virt + tm->tm_len) <= (virt + len))) {
				/* We have overlap - must reject the mapping */
				return ANANAS_ERROR(BAD_ADDRESS);
			}
		}
	}

	struct THREAD_MAPPING* tm = kmalloc(sizeof(*tm));
	memset(tm, 0, sizeof(*tm));

	/*
	 * XXX We should ask the VM for some kind of reservation of the
	 * THREAD_MAP_ALLOC flag is set; now we'll just assume that the
	 * memory is there...
	 */

	DQUEUE_INIT(&tm->tm_pages);
	tm->tm_virt = virt;
	tm->tm_len = len;
	tm->tm_flags = flags;
	DQUEUE_ADD_TAIL(&t->t_mappings, tm);
	TRACE(THREAD, INFO, "thread_mapto(): t=%p, tm=%p, phys=%p, virt=%p, flags=0x%x", t, tm, phys, virt, flags);

	md_thread_map(t, (void*)tm->tm_virt, (void*)phys, len, (flags & (THREAD_MAP_LAZY | THREAD_MAP_ALLOC)) ? 0 : thread_make_vmflags(flags));
	*out = tm;
	return ANANAS_ERROR_OK;
}

/*
 * Maps a piece of kernel memory 'from' for a thread 't'; returns the
 * address where it was mapped to. Calls thread_mapto(), so refer to
 * flags there.
 */
errorcode_t
thread_map(thread_t* t, addr_t phys, size_t len, uint32_t flags, struct THREAD_MAPPING** out)
{
	/*
	 * Locate a new address to map to; we currently never re-use old addresses.
	 */
	addr_t virt = t->t_next_mapping;
	t->t_next_mapping += len;
	if ((t->t_next_mapping & (PAGE_SIZE - 1)) > 0)
		t->t_next_mapping += PAGE_SIZE - (t->t_next_mapping & (PAGE_SIZE - 1));
	return thread_mapto(t, virt, phys, len, flags, out);
}

errorcode_t
thread_handle_fault(thread_t* t, addr_t virt, int flags)
{
	TRACE(THREAD, INFO, "thread_handle_fault(): thread=%p, virt=%p, flags=0x%x", t, virt, flags);

	/* Walk through the mappings one by one */
	if (!DQUEUE_EMPTY(&t->t_mappings)) {
		DQUEUE_FOREACH(&t->t_mappings, tm, struct THREAD_MAPPING) {
			if (!(virt >= tm->tm_virt && (virt < (tm->tm_virt + tm->tm_len))))
				continue;

			/* Fetch a page */
			struct PAGE* p = page_alloc_single();
			if (p == NULL)
				return ANANAS_ERROR(OUT_OF_MEMORY);
			DQUEUE_ADD_TAIL(&tm->tm_pages, p);
			p->p_addr = virt & ~(PAGE_SIZE - 1);

			/* Map the page */
			md_thread_map(t, (void*)p->p_addr, (void*)page_get_paddr(p), 1, thread_make_vmflags(tm->tm_flags));
			errorcode_t err = ANANAS_ERROR_OK;
			if (tm->tm_fault != NULL) {
				/* Invoke the mapping-specific fault handler */
				err = tm->tm_fault(t, tm, virt);
				if (err != ANANAS_ERROR_NONE) {
					/* Mapping failed; throw the thread mapping away and nuke the page */
					md_thread_unmap(t, p->p_addr, 1);
					DQUEUE_REMOVE(&tm->tm_pages, p);
					page_free(p);
				}
			}
			return err;
		}
	}

	return ANANAS_ERROR(BAD_ADDRESS);
}

#if 0
errorcode_t
thread_unmap(thread_t* t, void* ptr, size_t len)
{
	struct THREAD_MAPPING* tm = t->t_mappings;
	struct THREAD_MAPPING* prev = NULL;
	while (tm != NULL) {
		if (tm->start == (addr_t)ptr && tm->len == len) {
			/* found it */
			if(prev == NULL)
				t->t_mappings = tm->next;
			else
				prev->next = tm->next;
			errorcode_t err = md_thread_unmap(t, (void*)tm->start, tm->len);
			ANANAS_ERROR_RETURN(err);
			kfree(tm->ptr);
			kfree(tm);
			return ANANAS_ERROR_OK;
		}
		prev = tm; tm = tm->next;
	}
	return ANANAS_ERROR(BAD_ADDRESS);
}
#endif

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

	/* Free as much of the thread as we can */
	thread_free(thread);

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

	/*
	 * OK; we have a fresh thread. Copy all memory mappings over (except private
	 * mappings)
	 * XXX we could just re-add the mapping; but this needs refcounting etc...
	 * and only works for readonly things XXX
	 */
	DQUEUE_FOREACH(&parent->t_mappings, tm, struct THREAD_MAPPING) {
		if (tm->tm_flags & THREAD_MAP_PRIVATE)
			continue;
		struct THREAD_MAPPING* ttm;
		err = thread_mapto(t, tm->tm_virt, (addr_t)NULL, tm->tm_len, THREAD_MAP_ALLOC | (tm->tm_flags), &ttm);
		if (err != ANANAS_ERROR_NONE) {
			thread_free(t);
			thread_destroy(t);
			return err;
		}

		/* Copy the mapping-specific parts */
		ttm->tm_privdata = NULL; /* to be filled out by clone */
		ttm->tm_fault = tm->tm_fault;
		ttm->tm_destroy = tm->tm_destroy;
		ttm->tm_clone = tm->tm_clone;
		if (tm->tm_clone != NULL) {
			err = tm->tm_clone(t, ttm, tm);
			if (err != ANANAS_ERROR_OK) {
				thread_free(t);
				thread_destroy(t);
				return err;
			}
		}

		/*
		 * Copy the page one by one; skip pages that cannot be copied XXX This is
		 * horribly slow, we should use a copy-on-write mechanism.
		 *
		 * This loop assumes that our current process is the parent; thread_clone()
		 * asserts on this.
		 */
		if (!DQUEUE_EMPTY(&tm->tm_pages)) {
			DQUEUE_FOREACH(&tm->tm_pages, p, struct PAGE) {
				struct PAGE* new_page = page_alloc_order(p->p_order);
				if (new_page == NULL) {
					thread_free(t);
					thread_destroy(t);
					return ANANAS_ERROR(OUT_OF_MEMORY);
				}
				DQUEUE_ADD_TAIL(&t->t_pages, new_page);
				new_page->p_addr = p->p_addr;
				int num_pages = 1 << p->p_order;

				/* XXX make a temporary mapping to copy the data. We should do a copy-on-write */
				void* ktmp = kmem_map(page_get_paddr(new_page), num_pages * PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_KERNEL);
				memcpy(ktmp, (void*)p->p_addr, num_pages * PAGE_SIZE);
				kmem_unmap(ktmp, num_pages * PAGE_SIZE);
				
				/* Mark the page as present in the cloned process */
				md_thread_map(t, (void*)p->p_addr, (void*)page_get_paddr(new_page), num_pages, thread_make_vmflags(tm->tm_flags));
			}
		}
	}

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
idle_thread()
{
	while(1) {
		md_cpu_relax();
	}
}

#ifdef OPTION_KDB
extern struct THREAD* kdb_curthread;

void blaat(int flags)
{
#define FLAG_HANDLE 1
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

KDB_COMMAND(threads, "[s:flags]", "Displays current threads")
{
	int flags = 0;

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

	blaat(flags);
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
	if (!DQUEUE_EMPTY(&thread->t_mappings)) {
		DQUEUE_FOREACH(&thread->t_mappings, tm, struct THREAD_MAPPING) {
			kprintf("   flags      : 0x%x\n", tm->tm_flags);
			kprintf("   virtual    : 0x%x - 0x%x\n", tm->tm_virt, tm->tm_virt + tm->tm_len);
			kprintf("   length     : %u\n", tm->tm_len);
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

	struct THREAD* thread = kdb_curthread;
	for (int x = 0; x <= 32; x += 8)
		kprintf("rbp %d -> %p\n", x, *(register_t*)(thread->md_rsp + x));

	register_t rbp = *(register_t*)(thread->md_rsp + 24);
	kprintf("rbp %p rsp %p\n", rbp, thread->md_rsp);
  while(rbp >= 0xffff880000000000) {
    kprintf("[%p] ", *(uint64_t*)(rbp + 8));
    rbp = *(uint64_t*)rbp;
  }

}

KDB_COMMAND(curthread, "i:thread", "Sets current thread")
{
	kdb_curthread = (struct THREAD*)arg[1].a_u.u_value;
}
#endif /* KDB */

/* vim:set ts=2 sw=2: */
