#include <ananas/types.h>
#include <machine/param.h>
#include <ananas/console.h>
#include <ananas/device.h>
#include <ananas/handle.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <ananas/thread.h>
#include <ananas/threadinfo.h>
#include <ananas/lib.h>
#include <ananas/mm.h>

struct THREAD* threads = NULL;

void
thread_init(thread_t t, thread_t parent)
{
	memset(t, 0, sizeof(struct THREAD));
	t->mappings = NULL;
	t->flags = THREAD_FLAG_SUSPENDED;
	t->thread_handle = handle_alloc(HANDLE_TYPE_THREAD, t /* XXX should be parent? */);
	t->thread_handle->data.thread = t;

	if (parent != NULL) {
		t->path_handle = handle_clone(parent->path_handle);
	} else {
		/* No parent; use / as current path */
		t->path_handle = handle_alloc(HANDLE_TYPE_FILE, t);
		if (!vfs_open("/", NULL, &t->path_handle->data.vfs_file)) {
			kprintf("couldn't reopen root path handle\n");
		}
	}

	md_thread_init(t);

	/* initialize thread information structure and map it */
	t->threadinfo = kmalloc(sizeof(struct THREADINFO));
	memset(t->threadinfo, 0, sizeof(t->threadinfo));
	t->threadinfo->ti_size = sizeof(struct THREADINFO);
	t->threadinfo->ti_handle = t->thread_handle;
	t->threadinfo->ti_handle_stdin  = handle_alloc(HANDLE_TYPE_FILE, t);
	t->threadinfo->ti_handle_stdout = handle_alloc(HANDLE_TYPE_FILE, t);
	t->threadinfo->ti_handle_stderr = handle_alloc(HANDLE_TYPE_FILE, t);
	((struct HANDLE*)t->threadinfo->ti_handle_stdin)->data.vfs_file.device  = console_tty;
	((struct HANDLE*)t->threadinfo->ti_handle_stdout)->data.vfs_file.device = console_tty;
	((struct HANDLE*)t->threadinfo->ti_handle_stderr)->data.vfs_file.device = console_tty;
	struct THREAD_MAPPING* tm = thread_map(t, t->threadinfo, sizeof(struct THREADINFO), 0);
	md_thread_set_argument(t, tm->start);

	if (threads == NULL) {
		threads = t;
	} else {
		/* prepend t to the chain of threads */
		t->next = threads;
		threads->prev = t;
		threads = t;
	}
}

thread_t
thread_alloc(thread_t parent)
{
	thread_t t = kmalloc(sizeof(struct THREAD));
	thread_init(t, parent);
	return t;
}

void
thread_free_mappings(thread_t t)
{
	/*
	 * Free all mapped process memory, if needed. We don't bother to unmap them
	 * in the thread's VM as it's going away anyway XXX we should for !x86
	 */
	struct THREAD_MAPPING* tm = t->mappings;
	struct THREAD_MAPPING* next;
	while (tm != NULL) {
		if (tm->flags & THREAD_MAP_ALLOC)
			kfree((void*)tm->ptr);
		next = tm->next;
		kfree(tm);
		tm = next;
	}

}

void
thread_free(thread_t t)
{
	/* free all thread mappings */
	thread_free_mappings(t);

	/* free all handles in use by the thread */
	while (t->handle != NULL)
		handle_free(t->handle);

	/* free the machine-dependant bits */
	md_thread_free(t);
}

void
thread_destroy(thread_t t)
{
	/* XXX lock */
	if (threads == t) {
		threads = t->next;
		threads->prev = NULL;
	} else {
		threads->prev = t->next;
		t->next->prev = threads->prev;
	}
	/* XXX unlock */

	kfree(t);
}

/*
 * Maps a piece of kernel memory 'from' to 'to' for thread 't'.
 * 'len' is the length in bytes; 'flags' contains mapping flags:
 *  THREAD_MAP_ALLOC - allocate a new piece of memory (will be zeroed first)
 * Returns the new mapping structure
 */
struct THREAD_MAPPING*
thread_mapto(thread_t t, void* to, void* from, size_t len, uint32_t flags)
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
	tm->next = t->mappings;
	t->mappings = tm;

	md_thread_map(t, to, from, len, 0);
	return tm;
}

/*
 * Locate a thread mapping, or 0 if the memory is not mapped.
 */
addr_t
thread_find_mapping(thread_t t, void* addr)
{
	addr_t address = (addr_t)addr & ~(PAGE_SIZE - 1);
	struct THREAD_MAPPING* tm = t->mappings;
	while (tm != NULL) {
		if ((address >= (addr_t)tm->start) && (address <= (addr_t)(tm->start + tm->len))) {
			return (addr_t)tm->ptr + ((addr_t)addr - (addr_t)tm->start);
		}
		tm = tm->next;
	}
	return 0;
}

/*
 * Maps a piece of kernel memory 'from' for a thread 't'; returns the
 * address where it was mapped to. Calls thread_mapto(), so refer to
 * flags there.
 */
struct THREAD_MAPPING*
thread_map(thread_t t, void* from, size_t len, uint32_t flags)
{
	/*
	 * Locate a new address to map to; we currently never re-use old addresses.
	 */
	void* addr = (void*)t->next_mapping;
	t->next_mapping += len;
	if ((t->next_mapping & (PAGE_SIZE - 1)) > 0)
		t->next_mapping += PAGE_SIZE - (t->next_mapping & (PAGE_SIZE - 1));
	return thread_mapto(t, addr, from, len, flags);
}

int
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
			md_thread_unmap(t, (void*)tm->start, tm->len);
			kfree(tm->ptr);
			kfree(tm);
			return 1;
		}
		prev = tm; tm = tm->next;
	}
	return 0;
}

void
thread_suspend(thread_t t)
{
	KASSERT((t->flags & THREAD_FLAG_SUSPENDED) == 0, "suspending suspended thread");
	t->flags |= THREAD_FLAG_SUSPENDED;
}

void
thread_resume(thread_t t)
{
	KASSERT(t->flags & THREAD_FLAG_SUSPENDED, "resuming active thread");
	KASSERT((t->flags & THREAD_FLAG_TERMINATING) == 0, "resuming terminating thread");
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
thread_dump()
{
	struct THREAD* t = threads;
	struct THREAD* cur = PCPU_CURTHREAD();
	kprintf("thread dump\n");
	while (t != NULL) {
		kprintf ("%p: flags [", t);
		if (t->flags & THREAD_FLAG_ACTIVE)    kprintf(" active");
		if (t->flags & THREAD_FLAG_SUSPENDED) kprintf(" suspended");
		if (t->flags & THREAD_FLAG_TERMINATING) kprintf(" terminating");
		kprintf(" ]%s\n", (t == cur) ? " <- current" : "");
		t = t->next;
	}
}

struct THREAD*
thread_clone(struct THREAD* parent, int flags)
{
	struct THREAD* t = thread_alloc(parent);
	if (t == NULL)
		return NULL;

	/*
	 * OK; we have a fresh thread. Copy all memory mappings over
	 * XXX we could just re-add the mapping; but this needs refcounting etc... XXX
	 */
	for (struct THREAD_MAPPING* tm = parent->mappings; tm != NULL; tm = tm->next) {
		struct THREAD_MAPPING* ttm = thread_mapto(t, (void*)tm->start, NULL, tm->len, THREAD_MAP_ALLOC);
		memcpy(ttm->ptr, tm->ptr, tm->len);
	}

	/*
	 * Must copy the thread state over; note that this is the
	 * result of a system call, so we want to influence the
	 * return value.
	 */
	md_thread_clone(t, parent, 0);

	/* Thread is ready to rock */
	thread_resume(t);
	return t;
}

void
thread_set_args(thread_t t, const char* args)
{
	unsigned int i;
	for (i = 0; i < THREADINFO_ARGS_LENGTH - 1; i++)
		if(args[i] == '\0' && args[i + 1] == '\0')
			break;
	if (i == THREADINFO_ARGS_LENGTH)
		panic("thread_set_args(): args must be doubly nul-terminated");

	memcpy(t->threadinfo->ti_args, args, i + 2 /* terminating \0\0 */);
}

void
thread_set_errorcode(thread_t t, errorcode_t code)
{
	t->threadinfo->ti_errorcode = code;
}

/* vim:set ts=2 sw=2: */
