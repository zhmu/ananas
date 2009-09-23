#include "types.h"

#ifndef __THREAD_H__
#define __THREAD_H__

typedef struct THREAD* thread_t;

struct THREAD {
	/*
	 * Machine-dependant data - must be first.
	 */
	void* md;
	struct THREAD* prev;
	struct THREAD* next;

	unsigned int tid;		/* Thread ID */
};

/* Machine-dependant callback to initialize a thread */
int md_thread_init(thread_t thread);

/* Retrieve the size of the machine-dependant structure */
size_t md_thread_get_privdata_length();

/* Machine-dependant callback to destroy a thread */
void md_thread_destroy(thread_t thread);

thread_t thread_alloc();
void thread_free(thread_t);
void md_thread_switch(thread_t thread);

#endif
