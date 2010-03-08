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
	if (threads == t) {
		threads = t->next;
		threads->prev = NULL;
	} else {
		threads->prev = t->next;
		t->next->prev = threads->prev;
	}

	md_thread_destroy(t);
	kfree(t);
}

void*
thread_map(thread_t t, size_t len, uint32_t flags)
{
	struct THREAD_MAPPING* tm = kmalloc(sizeof(*tm));
	memset(tm, 0, sizeof(*tm));

	void* addr = (void*)t->next_mapping;
	t->next_mapping += len / PAGE_SIZE;
	if (len % PAGE_SIZE) t->next_mapping += PAGE_SIZE;

	tm->ptr = kmalloc(len);
	tm->start = addr;
	tm->len = len;
	tm->next = t->mappings;
	t->mappings = tm;

	/*kprintf("mapping: '%p' => userland '%p'; len=%u\n", tm->ptr, addr, len);*/
	md_thread_map(t, addr, tm->ptr, len, 0);
	return (void*)tm->start;
}

int
thread_unmap(thread_t t, void* ptr, size_t len)
{
	struct THREAD_MAPPING* tm = t->mappings;
	struct THREAD_MAPPING* prev = NULL;
	while (tm != NULL) {
		if (tm->start == (addr_t)ptr && tm->len == len) {
			/* found it */
			if(prev = NULL)
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
