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
#include <ananas/lib.h>
#include <ananas/mm.h>

TRACE_SETUP;

struct THREAD* threads = NULL;

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
		 * to do.
		 */
		err = handle_alloc(HANDLE_TYPE_FILE, t, &t->path_handle);
		if (err != ANANAS_ERROR_NONE) {
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
	((struct HANDLE*)t->threadinfo->ti_handle_stdin)->data.vfs_file.device  = console_tty;
	((struct HANDLE*)t->threadinfo->ti_handle_stdout)->data.vfs_file.device = console_tty;
	((struct HANDLE*)t->threadinfo->ti_handle_stderr)->data.vfs_file.device = console_tty;
	struct THREAD_MAPPING* tm;
	err = thread_map(t, t->threadinfo, sizeof(struct THREADINFO), 0, &tm);
	if (err != ANANAS_ERROR_NONE)
		goto fail;
	md_thread_set_argument(t, tm->start);

	/* XXX lock */
	if (threads == NULL) {
		threads = t;
	} else {
		/* prepend t to the chain of threads */
		t->next = threads;
		threads->prev = t;
		threads = t;
	}
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
	DQUEUE_REMOVE(&t->mappings, tm);
	if (tm->flags & THREAD_MAP_ALLOC)
		kfree((void*)tm->ptr);
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
	 Free all thread mappings; this must be done afterwards because memory handles are
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

	/* Free the machine-dependant bits */
	md_thread_free(t);

	/* Thread is alive, yet lingering... */
	t->flags |= THREAD_FLAG_ZOMBIE;
}

void
thread_destroy(thread_t t)
{
	KASSERT(t->flags & THREAD_FLAG_ZOMBIE, "thread_destroy() on a non-zombie thread");

	/* XXX lock */
	if (threads == t) {
		threads = t->next;
		threads->prev = NULL;
	} else {
		threads->prev = t->next;
		t->next->prev = threads->prev;
	}
	/* XXX unlock */

	/* Ensure the thread handle itself won't have a thread anymore */
	t->thread_handle->thread = NULL;
	handle_destroy(t->thread_handle, 0);
	kfree(t);
}

/*
 * Maps a piece of kernel memory 'from' to 'to' for thread 't'.
 * 'len' is the length in bytes; 'flags' contains mapping flags:
 *  THREAD_MAP_ALLOC - allocate a new piece of memory (will be zeroed first)
 * Returns the new mapping structure
 */
errorcode_t
thread_mapto(thread_t t, void* to, void* from, size_t len, uint32_t flags, struct THREAD_MAPPING** out)
{
	struct THREAD_MAPPING* tm = kmalloc(sizeof(*tm));
	memset(tm, 0, sizeof(*tm));

	if (flags & THREAD_MAP_ALLOC) {
		from = kmalloc(len);
		memset(from, 0, len);
	}

	tm->ptr = from;
	tm->start = (addr_t)to;
	tm->len = len;
	tm->flags = flags;
	DQUEUE_ADD_TAIL(&t->mappings, tm);

	md_thread_map(t, to, from, len, 0);
	*out = tm;
	return ANANAS_ERROR_OK;
}

/*
 * Locate a thread mapping, or 0 if the memory is not mapped.
 */
addr_t
thread_find_mapping(thread_t t, void* addr)
{
	addr_t address = (addr_t)addr & ~(PAGE_SIZE - 1);
	DQUEUE_FOREACH(&t->mappings, tm, struct THREAD_MAPPING) {
		if ((address >= (addr_t)tm->start) && (address <= (addr_t)(tm->start + tm->len))) {
			return (addr_t)tm->ptr + ((addr_t)addr - (addr_t)tm->start);
		}
	}
	return 0;
}

/*
 * Maps a piece of kernel memory 'from' for a thread 't'; returns the
 * address where it was mapped to. Calls thread_mapto(), so refer to
 * flags there.
 */
errorcode_t
thread_map(thread_t t, void* from, size_t len, uint32_t flags, struct THREAD_MAPPING** out)
{
	/*
	 * Locate a new address to map to; we currently never re-use old addresses.
	 */
	void* addr = (void*)t->next_mapping;
	t->next_mapping += len;
	if ((t->next_mapping & (PAGE_SIZE - 1)) > 0)
		t->next_mapping += PAGE_SIZE - (t->next_mapping & (PAGE_SIZE - 1));
	return thread_mapto(t, addr, from, len, flags, out);
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
	if (t == NULL)
		return;
	t->flags |= THREAD_FLAG_SUSPENDED;
}

void
thread_resume(thread_t t)
{
	if (t == NULL)
		return;
	KASSERT((t->flags & THREAD_FLAG_TERMINATING) == 0, "resuming terminating thread %p", t);
	t->flags &= ~THREAD_FLAG_SUSPENDED;
}
	
void
thread_exit(int exitcode)
{
	thread_t thread = PCPU_GET(curthread);
	KASSERT(thread != NULL, "thread_exit() without thread");
	KASSERT((thread->flags & THREAD_FLAG_TERMINATING) == 0, "exiting already termating thread");

	/*
	 * Mark the thread as suspended and terminating; the thread will remain
	 * in the terminating status until it is queried about its status.
	 */
	thread->flags |= THREAD_FLAG_SUSPENDED | THREAD_FLAG_TERMINATING;
	thread->terminate_info = exitcode;

	/* disown the current thread, it is no longer schedulable */
	PCPU_SET(curthread, NULL);

	/* free as much of the thread as we can */
	thread_free(thread);

	/* signal anyone waiting on us */
	handle_signal(thread->thread_handle, THREAD_EVENT_EXIT, thread->terminate_info);

	/* force a context switch - won't return */
	schedule();
}

void
thread_dump(int num_args, char** arg)
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

	struct THREAD* t = threads;
	struct THREAD* cur = PCPU_CURTHREAD();
	kprintf("thread dump\n");
	while (t != NULL) {
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
		t = t->next;
	}
}

errorcode_t
thread_clone(struct THREAD* parent, int flags, struct THREAD** dest)
{
	errorcode_t err;

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
		err = thread_mapto(t, (void*)tm->start, NULL, tm->len, THREAD_MAP_ALLOC, &ttm);
		if (err != ANANAS_ERROR_NONE) {
			thread_free(t);
			thread_destroy(t);
			return err;
		}
		memcpy(ttm->ptr, tm->ptr, tm->len);
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

/* vim:set ts=2 sw=2: */
