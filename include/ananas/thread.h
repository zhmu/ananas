#include <ananas/types.h>
#include <ananas/vfs.h>
#include <ananas/limits.h>
#include <machine/thread.h>

#ifndef __THREAD_H__
#define __THREAD_H__

typedef struct THREAD* thread_t;
typedef void (*kthread_func_t)(void);
struct THREADINFO;

#define THREAD_EVENT_EXIT 1

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
#define THREAD_FLAG_TERMINATING	0x0004	/* Thread is terminating */

	unsigned int terminate_info;
#define THREAD_MAKE_EXITCODE(a,b) (((a) << 24) | ((b) & 0x00ffffff))
#define THREAD_TERM_SYSCALL	0x1	/* euthanasia */
#define THREAD_TERM_FAULT	0x2	/* programming fault */
#define THREAD_TERM_FAILURE	0x3	/* generic failure */

	addr_t next_mapping;		/* address of next mapping */
	struct THREAD_MAPPING* mappings;

	struct THREADINFO* threadinfo;	/* Thread startup information */
	struct HANDLE* thread_handle;	/* Handle identifying this thread */
	struct HANDLE* handle;		/* First handle */

	struct HANDLE* path_handle;	/* Current path */
};

/* Machine-dependant callback to initialize a thread */
errorcode_t md_thread_init(thread_t thread);

/* Machine-dependant callback to free thread data */
void md_thread_free(thread_t thread);

/* Machine-dependant kernel thread activation */
void md_thread_setkthread(thread_t thread, kthread_func_t kfunc);

errorcode_t thread_init(thread_t t, thread_t parent);
errorcode_t thread_alloc(thread_t parent, thread_t* dest);
void thread_free(thread_t);
void thread_destroy(thread_t);
errorcode_t thread_set_args(thread_t t, const char* args, size_t args_len);
errorcode_t thread_set_environment(thread_t t, const char* env, size_t env_len);

void md_thread_switch(thread_t new, thread_t old);
void md_idle_thread();

#define THREAD_MAP_READ 	0x01
#define THREAD_MAP_WRITE	0x02
#define THREAD_MAP_EXECUTE	0x04
void md_thread_set_entrypoint(thread_t thread, addr_t entry);
void md_thread_set_argument(thread_t thread, addr_t arg);
void* md_thread_map(thread_t thread, void* to, void* from, size_t length, int flags);
errorcode_t thread_unmap(thread_t t, void* ptr, size_t len);
void* md_map_thread_memory(thread_t thread, void* ptr, size_t length, int write);
void md_thread_clone(struct THREAD* t, struct THREAD* parent, register_t retval);
errorcode_t md_thread_unmap(thread_t thread, void* addr, size_t length);

#define THREAD_MAP_ALLOC 0x800
errorcode_t thread_mapto(thread_t t, void* to, void* from, size_t len, uint32_t flags, struct THREAD_MAPPING** out);
errorcode_t thread_map(thread_t t, void* from, size_t len, uint32_t flags, struct THREAD_MAPPING** out);
errorcode_t thread_unmap(thread_t t, void* ptr, size_t len);
addr_t thread_find_mapping(thread_t t, void* addr);
void thread_free_mappings(thread_t t);

void thread_suspend(thread_t t);
void thread_resume(thread_t t);
void thread_exit(int exitcode);
void thread_dump();
errorcode_t thread_clone(struct THREAD* parent, int flags, struct THREAD** dest);

#endif
