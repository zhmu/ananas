#include <sys/types.h>
#include <sys/vfs.h>
#include <machine/thread.h>

#ifndef __THREAD_H__
#define __THREAD_H__

#define THREAD_MAX_FILES	16

typedef struct THREAD* thread_t;

struct THREAD_MAPPING {
	uint32_t	flags;
	addr_t		start;			/* userland address */
	size_t		len;			/* length */
	void*		ptr;			/* kernel ptr */
	struct		THREAD_MAPPING* next;
};

struct THREAD {
	/* Machine-dependant data - must be first */
	MD_THREAD_FIELDS
	struct THREAD* prev;
	struct THREAD* next;

	unsigned int flags;
#define THREAD_FLAG_ACTIVE	0x0001	/* Thread is scheduled somewhere */
#define THREAD_FLAG_SUSPENDED	0x0002	/* Thread is currently suspended */
	unsigned int tid;		/* Thread ID */

	addr_t next_mapping;		/* address of next mapping */
	struct THREAD_MAPPING* mappings;

	struct VFS_FILE file[THREAD_MAX_FILES];	/* Opened files */
};

/* Machine-dependant callback to initialize a thread */
int md_thread_init(thread_t thread);

/* Machine-dependant callback to destroy a thread */
void md_thread_destroy(thread_t thread);

/* Machine-dependant idle thread activation */
void md_thread_setidle(thread_t thread);

void thread_init(thread_t t);
thread_t thread_alloc();
void thread_free(thread_t);
void md_thread_switch(thread_t new, thread_t old);

void md_thread_set_entrypoint(thread_t thread, addr_t entry);
void* md_thread_map(thread_t thread, void* to, void* from, size_t length, int flags);
int md_thread_unmap(thread_t thread, void* addr, size_t length);
void* md_map_thread_memory(thread_t thread, void* ptr, size_t length, int write);

#define THREAD_MAP_ALLOC 0x800
struct THREAD_MAPPING* thread_mapto(thread_t t, void* to, void* from, size_t len, uint32_t flags);
struct THREAD_MAPPING* thread_map(thread_t t, void* from, size_t len, uint32_t flags);
int thread_unmap(thread_t t, void* ptr, size_t len);

void thread_suspend(thread_t t);
void thread_resume(thread_t t);
void thread_exit();
void thread_dump();

#endif
