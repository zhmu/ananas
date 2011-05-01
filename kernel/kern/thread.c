#include <ananas/types.h>
#include <machine/param.h>
#include <ananas/console.h>
#include <ananas/device.h>
#include <ananas/error.h>
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

static struct SPINLOCK spl_threadqueue;
static struct THREAD_QUEUE threadqueue;

errorcode_t
thread_init(thread_t t, thread_t parent)
{
	errorcode_t err;

	memset(t, 0, sizeof(struct THREAD));
	DQUEUE_INIT(&t->mappings);
	t->flags = THREAD_FLAG_SUSPENDED;
	err = handle_alloc(HANDLE_TYPE_THREAD, t /* XXX should be parent? */, &t->thread_handle);
	ANANAS_ERROR_RETURN(err);
	t->thread_handle->data.thread = t;

	if (parent != NULL) {
		err = handle_clone(t, parent->path_handle, &t->path_handle);
		if (err != ANANAS_ERROR_NONE) {
			/*
			 * XXX Unable to clone parent's path - what now? Our VFS isn't mature enough
			 * to deal with abandoned handles (or even abandon handles in the first
			 * place), so this should never, ever happen.
			 */
			panic("thread_init(): could not clone root path");
		}
	} else {
		/*
		 * No parent; use / as current path. This will not work in very early
		 * initialiation, but that is fine - our lookup code should know what
		 * to do with the NULL backing inode.
		 */
		err = handle_alloc(HANDLE_TYPE_FILE, t, &t->path_handle);
		if (err == ANANAS_ERROR_NONE) {
			err = vfs_open("/", NULL, &t->path_handle->data.vfs_file);
		}
	}

	md_thread_init(t);

	/* initialize thread information structure and map it */
	t->threadinfo = kmalloc(sizeof(struct THREADINFO));
	memset(t->threadinfo, 0, sizeof(t->threadinfo));
	t->threadinfo->ti_size = sizeof(struct THREADINFO);
	t->threadinfo->ti_handle = t->thread_handle;
	err = handle_alloc(HANDLE_TYPE_FILE, t, (struct HANDLE**)&t->threadinfo->ti_handle_stdin);
	if (err != ANANAS_ERROR_NONE)
		goto fail;
	err = handle_alloc(HANDLE_TYPE_FILE, t, (struct HANDLE**)&t->threadinfo->ti_handle_stdout);
	if (err != ANANAS_ERROR_NONE)
		goto fail;
	err = handle_alloc(HANDLE_TYPE_FILE, t, (struct HANDLE**)&t->threadinfo->ti_handle_stderr);
	if (err != ANANAS_ERROR_NONE)
		goto fail;
	if (parent != NULL)
		thread_set_environment(t, parent->threadinfo->ti_env, PAGE_SIZE /* XXX */);

	TRACE(THREAD, INFO, "t=%p, stdin=%p, stdout=%p, stderr=%p",
	 t,
	 t->threadinfo->ti_handle_stdin,
	 t->threadinfo->ti_handle_stdout,
	 t->threadinfo->ti_handle_stderr);


	/*
	 * XXX hook the console as our std{in,out,err} handles; this is not correct,
	 * it should be inherited from the parent
	 */
	((struct HANDLE*)t->threadinfo->ti_handle_stdin)->data.vfs_file.f_device  = console_tty;
	((struct HANDLE*)t->threadinfo->ti_handle_stdout)->data.vfs_file.f_device = console_tty;
	((struct HANDLE*)t->threadinfo->ti_handle_stderr)->data.vfs_file.f_device = console_tty;
	struct THREAD_MAPPING* tm;
	err = thread_map(t, KVTOP((addr_t)t->threadinfo), sizeof(struct THREADINFO), VM_FLAG_READ | VM_FLAG_WRITE, &tm);
	if (err != ANANAS_ERROR_NONE)
		goto fail;
	md_thread_set_argument(t, tm->tm_virt);

	/* Initialize scheduler-specific parts */
	scheduler_init_thread(t);

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
thread_alloc(thread_t parent, thread_t* dest)
{
	thread_t t = kmalloc(sizeof(struct THREAD));
	errorcode_t err = thread_init(t, parent);
	if (err == ANANAS_ERROR_NONE)
		*dest = t;
	return err;
}

void
thread_free_mapping(thread_t t, struct THREAD_MAPPING* tm)
{
	TRACE(THREAD, INFO, "thread_free_mapping(): t=%p tm=%p tm->phys=%p", t, tm, tm->tm_phys);
	DQUEUE_REMOVE(&t->mappings, tm);
	if (tm->tm_destroy != NULL)
		tm->tm_destroy(t, tm);
	if (tm->tm_flags & THREAD_MAP_ALLOC)
		kmem_free((void*)tm->tm_phys);
	kfree(tm);
}

void
thread_free_mappings(thread_t t)
{
	/*
	 * Free all mapped process memory, if needed. We don't bother to unmap them
	 * in the thread's VM as it's going away anyway XXX we should for !x86
	 */
	while(!DQUEUE_EMPTY(&t->mappings)) {
		struct THREAD_MAPPING* tm = DQUEUE_HEAD(&t->mappings);
		thread_free_mapping(t, tm); /* also removed the mapping */
	}
}

void
thread_free(thread_t t)
{
	/* If the thread is already freed, do not bother */
	if (t->flags & THREAD_FLAG_ZOMBIE)
		return;

	/*
	 * Free all handles in use by the thread. Note that we must not free the thread
	 * handle itself, as the thread isn't destroyed yet (we are just freeing all
	 * resources here - the thread handle itself is necessary for obtaining the
	 * result code etc)
	 */
	DQUEUE_FOREACH_SAFE(&t->handles, h, struct HANDLE) {
		if (h == t->thread_handle)
			continue;
		DQUEUE_REMOVE(&t->handles, h);
		handle_free(h);
	}
	KASSERT(!DQUEUE_EMPTY(&t->handles), "thread handle was not part of thread handle queue");

	/*
	 * Free all thread mappings; this must be done afterwards because memory handles are
	 * implemented as mappings and they are already gone by now
	 */
	thread_free_mappings(t);

	/*
	 * Mark the thread as terminating if it isn't already; having to do so means
	 * the thread's isn't exiting voluntary.
	 */
	if ((t->flags & THREAD_FLAG_TERMINATING) == 0) {
		t->flags |= THREAD_FLAG_TERMINATING;
		t->terminate_info = THREAD_MAKE_EXITCODE(THREAD_TERM_FAILURE, 0);
		handle_signal(t->thread_handle, THREAD_EVENT_EXIT, t->terminate_info);
	}

	/* Thread is alive, yet lingering... */
	t->flags |= THREAD_FLAG_ZOMBIE;

	/* If the thread was on the scheduler queue, remove it */
	if ((t->flags & THREAD_FLAG_SUSPENDED) == 0)
		scheduler_remove_thread(t);
}

void
thread_destroy(thread_t t)
{
	KASSERT(t->flags & THREAD_FLAG_ZOMBIE, "thread_destroy() on a non-zombie thread");

	/* Free the machine-dependant bits */
	md_thread_free(t);

	/* Remove the queue from our queue */
	spinlock_lock(&spl_threadqueue);
	DQUEUE_REMOVE(&threadqueue, t);
	spinlock_unlock(&spl_threadqueue);

	/* Ensure the thread handle itself won't have a thread anymore */
	t->thread_handle->thread = NULL;
	handle_destroy(t->thread_handle, 0);
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
	return vm_flags;
}

/*
 * Maps a piece of kernel memory 'from' to 'to' for thread 't'.
 * 'len' is the length in bytes; 'flags' contains mapping flags:
 *  THREAD_MAP_ALLOC - allocate a new piece of memory (will be zeroed first)
 * Returns the new mapping structure
 */
errorcode_t
thread_mapto(thread_t t, addr_t virt, addr_t phys, size_t len, uint32_t flags, struct THREAD_MAPPING** out)
{
	struct THREAD_MAPPING* tm = kmalloc(sizeof(*tm));
	memset(tm, 0, sizeof(*tm));

	if (flags & THREAD_MAP_ALLOC) {
		phys = (addr_t)kmem_alloc((len + PAGE_SIZE - 1) / PAGE_SIZE);
		/* XXX so how do we zero it out? */
	}

	tm->tm_phys = phys;
	tm->tm_virt = virt;
	tm->tm_len = len;
	tm->tm_flags = flags;
	DQUEUE_ADD_TAIL(&t->mappings, tm);
	TRACE(THREAD, INFO, "thread_mapto(): t=%p, tm=%p, phys=%p, virt=%p, flags=0x%x", t, tm, phys, virt, flags);

	md_thread_map(t, (void*)tm->tm_virt, (void*)tm->tm_phys, len, (flags & THREAD_MAP_LAZY) ? 0 : thread_make_vmflags(flags));
	*out = tm;
	return ANANAS_ERROR_OK;
}

/*
 * Maps a piece of kernel memory 'from' for a thread 't'; returns the
 * address where it was mapped to. Calls thread_mapto(), so refer to
 * flags there.
 */
errorcode_t
thread_map(thread_t t, addr_t phys, size_t len, uint32_t flags, struct THREAD_MAPPING** out)
{
	/*
	 * Locate a new address to map to; we currently never re-use old addresses.
	 */
	addr_t virt = t->next_mapping;
	t->next_mapping += len;
	if ((t->next_mapping & (PAGE_SIZE - 1)) > 0)
		t->next_mapping += PAGE_SIZE - (t->next_mapping & (PAGE_SIZE - 1));
	return thread_mapto(t, virt, phys, len, flags, out);
}

errorcode_t
thread_handle_fault(thread_t t, addr_t virt, int flags)
{
	TRACE(THREAD, INFO, "thread_handle_fault(): thread=%p, virt=%p, flags=0x%x", t, virt, flags);

	/* Walk through the mappings one by one */
	if (!DQUEUE_EMPTY(&t->mappings)) {
		DQUEUE_FOREACH(&t->mappings, tm, struct THREAD_MAPPING) {
			if (!(virt >= tm->tm_virt && (virt < (tm->tm_virt + tm->tm_len))))
				continue;
			if (tm->tm_fault == NULL)
				continue;

			/* Map the page */
			md_thread_map(t, (void*)virt, (void*)(tm->tm_phys + (virt - tm->tm_virt)), 1, thread_make_vmflags(tm->tm_flags));

			/* Mapping found; request it to map the page */
			return tm->tm_fault(t, tm, virt);
		}
	}
	return ANANAS_ERROR(BAD_ADDRESS);
}

#if 0
errorcode_t
thread_unmap(thread_t t, void* ptr, size_t len)
{
	struct THREAD_MAPPING* tm = t->mappings;
	struct THREAD_MAPPING* prev = NULL;
	while (tm != NULL) {
		if (tm->start == (addr_t)ptr && tm->len == len) {
			/* found it */
			if(prev == NULL)
				t->mappings = tm->next;
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
thread_suspend(thread_t t)
{
	if (t == NULL || (t->flags & THREAD_FLAG_SUSPENDED) != 0)
		return;
	scheduler_remove_thread(t);
	t->flags |= THREAD_FLAG_SUSPENDED;
}

void
thread_resume(thread_t t)
{
	if (t == NULL || (t->flags & THREAD_FLAG_SUSPENDED) == 0)
		return;
	KASSERT((t->flags & THREAD_FLAG_TERMINATING) == 0, "resuming terminating thread %p", t);
	t->flags &= ~THREAD_FLAG_SUSPENDED;
	scheduler_add_thread(t);
}
	
void
thread_exit(int exitcode)
{
	thread_t thread = PCPU_GET(curthread);
	KASSERT(thread != NULL, "thread_exit() without thread");
	KASSERT((thread->flags & THREAD_FLAG_TERMINATING) == 0, "exiting already termating thread");

	/*
	 * Mark the thread as terminating; the thread will remain in the terminating
	 * status until it is queried about its status.
	 */
	thread->flags |= THREAD_FLAG_TERMINATING;
	thread->terminate_info = exitcode;

	/* free as much of the thread as we can */
	thread_free(thread);

	/* signal anyone waiting on us */
	handle_signal(thread->thread_handle, THREAD_EVENT_EXIT, thread->terminate_info);

	/* disown the current thread, it is no longer schedulable */
	PCPU_SET(curthread, NULL);

	/* force a context switch - won't return */
	schedule();
}

errorcode_t
thread_clone(struct THREAD* parent, int flags, struct THREAD** dest)
{
	errorcode_t err;

	KASSERT(PCPU_GET(curthread) == parent, "thread_clone(): unsupported from non-current thread");

	struct THREAD* t;
	err = thread_alloc(parent, &t);
	ANANAS_ERROR_RETURN(err);

	/*
	 * OK; we have a fresh thread. Copy all memory mappings over
	 * XXX we could just re-add the mapping; but this needs refcounting etc... and only works for
	 * readonly things XXX
	 */
	DQUEUE_FOREACH(&parent->mappings, tm, struct THREAD_MAPPING) {
		struct THREAD_MAPPING* ttm;
		err = thread_mapto(t, tm->tm_virt, (addr_t)NULL, tm->tm_len, THREAD_MAP_ALLOC | (tm->tm_flags), &ttm);
		if (err != ANANAS_ERROR_NONE) {
			thread_free(t);
			thread_destroy(t);
			return err;
		}

		/*
		 * Copy the page one by one; skip pages that cannot be copied XXX This is
		 * horribly slow, we should use a copy-on-write mechanism (but that needs
		 * a much more complex VM than we currently have)
		 *
		 * This loop assumes that our current process is the parent; thread_clone()
		 * asserts on this.
		 */
		for (size_t n = 0; n < ROUND_UP(tm->tm_len, PAGE_SIZE); n += PAGE_SIZE) {
			/* If the page isn't mapped in the parent thread; skip it */
			addr_t va = md_thread_is_mapped(parent, tm->tm_virt + n, VM_FLAG_READ);
			if (va == 0)
				continue;

			/* XXX make a temporary mapping to copy the data. We should do a copy-on-write */
			void* ktmp = vm_map_kernel(ttm->tm_phys + n, 1, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_KERNEL);
			memcpy(ktmp, (void*)(tm->tm_virt + n), PAGE_SIZE);
			vm_unmap_kernel((addr_t)ktmp, 1);

			/* Mark the page as present in the cloned process */
			md_thread_map(t, (void*)(ttm->tm_virt + n), (void*)(ttm->tm_phys + n), 1, thread_make_vmflags(tm->tm_flags));

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
		}
	}

	/*
	 * Must copy the thread state over; note that this is the
	 * result of a system call, so we want to influence the
	 * return value.
	 */
	md_thread_clone(t, parent, ANANAS_ERROR(CLONED));

	/* Thread is ready to rock */
	thread_resume(t);
	*dest = t;
	return ANANAS_ERROR_OK;
}

errorcode_t
thread_set_args(thread_t t, const char* args, size_t args_len)
{
	for (unsigned int i = 0; i < ((THREADINFO_ARGS_LENGTH - 1) < args_len ? (THREADINFO_ARGS_LENGTH - 1) : args_len); i++)
		if(args[i] == '\0' && args[i + 1] == '\0') {
			memcpy(t->threadinfo->ti_args, args, i + 2 /* terminating \0\0 */);
			return ANANAS_ERROR_OK;
		}
	return ANANAS_ERROR(BAD_LENGTH);
}

errorcode_t
thread_set_environment(thread_t t, const char* env, size_t env_len)
{
	for (unsigned int i = 0; i < ((THREADINFO_ENV_LENGTH - 1) < env_len ? (THREADINFO_ENV_LENGTH - 1) : env_len); i++)
		if(env[i] == '\0' && env[i + 1] == '\0') {
			memcpy(t->threadinfo->ti_env, env, i + 2 /* terminating \0\0 */);
			return ANANAS_ERROR_OK;
		}

	return ANANAS_ERROR(BAD_LENGTH);
}

#ifdef KDB
extern struct THREAD* kdb_curthread;

void
kdb_cmd_threads(int num_args, char** arg)
{
	int flags = 0;
#define FLAG_HANDLE 1

	/* we use arguments as a mask to determine which information is to be dumped */
	for (int i = 1; i < num_args; i++) {
		for (char* ptr = arg[i]; *ptr != '\0'; ptr++)
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
			kprintf ("thread %p (handle %p): %s: flags [", t, t->thread_handle, t->threadinfo->ti_args);
			if (t->flags & THREAD_FLAG_ACTIVE)    kprintf(" active");
			if (t->flags & THREAD_FLAG_SUSPENDED) kprintf(" suspended");
			if (t->flags & THREAD_FLAG_TERMINATING) kprintf(" terminating");
			if (t->flags & THREAD_FLAG_ZOMBIE) kprintf(" zombie");
			kprintf(" ]%s\n", (t == cur) ? " <- current" : "");
			if (flags & FLAG_HANDLE) {
				kprintf("handles\n");
				if(!DQUEUE_EMPTY(&t->handles)) {
					DQUEUE_FOREACH_SAFE(&t->handles, handle, struct HANDLE) {
						kprintf(" handle %p, type %u\n",
						 handle, handle->type);
					}
				}
			}
		}
	spinlock_unlock(&spl_threadqueue);
}

void
kdb_cmd_thread(int num_args, char** arg)
{
	if (kdb_curthread == NULL) {
		kprintf("no current thread set\n");
		return;
	}

	struct THREAD* thread = kdb_curthread;
	kprintf("arg          : '%s'\n", thread->threadinfo->ti_args);
	kprintf("flags        : 0x%x\n", thread->flags);
	kprintf("terminateinfo: 0x%x\n", thread->terminate_info);
	kprintf("mappings:\n");
	if (!DQUEUE_EMPTY(&thread->mappings)) {
		DQUEUE_FOREACH(&thread->mappings, tm, struct THREAD_MAPPING) {
			kprintf("   flags      : 0x%x\n", tm->tm_flags);
			kprintf("   virtual    : 0x%x - 0x%x\n", tm->tm_virt, tm->tm_virt + tm->tm_len);
			kprintf("   physical   : 0x%x - 0x%x\n", tm->tm_phys, tm->tm_phys + tm->tm_len);
			kprintf("   length     : %u\n", tm->tm_len);
			kprintf("\n");
		}
	}
}

void
kdb_cmd_curthread(int num_args, char** arg)
{
	if (num_args != 2) {
		kprintf("need an argument\n");
		return;
	}

	char* ptr;
	addr_t addr = (addr_t)strtoul(arg[1], &ptr, 16);
	if (*ptr != '\0') {
		kprintf("parse error at '%s'\n", ptr);
		return;
	}

	kdb_curthread = (struct THREAD*)addr;
}
#endif /* KDB */

/* vim:set ts=2 sw=2: */
