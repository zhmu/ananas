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

/* vim:set ts=2 sw=2: */
