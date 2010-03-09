#include <types.h>
#include <machine/param.h>
#include "thread.h"
#include "lib.h"
#include "mm.h"

struct THREAD* threads = NULL;

thread_t
thread_alloc()
{
	thread_t t = kmalloc(sizeof(struct THREAD) + md_thread_get_privdata_length());
	memset(t, 0, sizeof(struct THREAD) + md_thread_get_privdata_length());
	t->md = (void*)((addr_t)t + sizeof(struct THREAD));
	t->mappings = NULL;

	md_thread_init(t);

	if (threads == NULL) {
		threads = t;
	} else {
		/* prepend t to the chain of threads */
		t->next = threads;
		threads->prev = t;
		threads = t;
	}

	return t;
}

void
thread_free(thread_t t)
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

	/*
	 * Free all mapped process memory, if needed. We don't bother to unmap them
	 * in the thread's VM as it's going away anyway.
	 */
	struct THREAD_MAPPING* tm = t->mappings;
	struct THREAD_MAPPING* next;
	while (tm != NULL) {
		if (tm->flags & THREAD_MAP_ALLOC)
			kfree((void*)tm->start);
		next = tm->next;
		kfree(tm);
		tm = next;
	}

	md_thread_destroy(t);
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
	t->next_mapping += len | (PAGE_SIZE - 1);
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
			/*kprintf("unmapping: userland '%p'; len=%u\n", tm->start, len);*/
			md_thread_unmap(t, (void*)tm->start, tm->len);
			kfree(tm->ptr);
			kfree(tm);
			return 1;
		}
		prev = tm; tm = tm->next;
	}
	return 0;
	
}

/* vim:set ts=2 sw=2: */
