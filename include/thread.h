#include "types.h"

#ifndef __THREAD_H__
#define __THREAD_H__

typedef struct THREAD* thread_t;

struct THREAD_MAPPING {
	uint32_t	flags;
	addr_t		start;			/* userland address */
	size_t		len;			/* length */
	void*		ptr;			/* kernel ptr */
	struct		THREAD_MAPPING* next;
};

struct THREAD {
	/*
	 * Machine-dependant data - must be first.
	 */
	void* md;
	struct THREAD* prev;
	struct THREAD* next;

	unsigned int flags;
#define THREAD_FLAG_ACTIVE	0x0001	/* Thread is scheduled somewhere */
	unsigned int tid;		/* Thread ID */

	addr_t next_mapping;		/* address of next mapping */

	struct THREAD_MAPPING* mappings;
};

/* Machine-dependant callback to initialize a thread */
int md_thread_init(thread_t thread);

/* Retrieve the size of the machine-dependant structure */
size_t md_thread_get_privdata_length();

/* Machine-dependant callback to destroy a thread */
void md_thread_destroy(thread_t thread);

thread_t thread_alloc();
void thread_free(thread_t);
void md_thread_switch(thread_t new, thread_t old);

void* md_thread_map(thread_t thread, void* to, void* from, size_t length, int flags);
int md_thread_unmap(thread_t thread, void* addr, size_t length);

#define THREAD_MAP_ALLOC 0x800
struct THREAD_MAPPING* thread_mapto(thread_t t, void* to, void* from, size_t len, uint32_t flags);
struct THREAD_MAPPING* thread_map(thread_t t, void* from, size_t len, uint32_t flags);

#endif
