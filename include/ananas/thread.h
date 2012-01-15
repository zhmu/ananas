#include <ananas/types.h>
#include <ananas/dqueue.h>
#include <ananas/vfs.h>
#include <ananas/limits.h>
#include <ananas/handle.h>
#include <ananas/init.h>
#include <ananas/schedule.h>
#include <machine/thread.h>

#ifndef __THREAD_H__
#define __THREAD_H__

typedef void (*kthread_func_t)(void*);
struct THREADINFO;
struct VFS_INODE;
struct THREAD_MAPPING;

#define THREAD_EVENT_EXIT 1
/* Fault function: handles a page fault for thread t, mapping tm and address virt */
typedef errorcode_t (*threadmap_fault_t)(thread_t* t, struct THREAD_MAPPING* tm, addr_t virt);

/* Clone function: copies mapping tsrc to tdest for thread t */
typedef errorcode_t (*threadmap_clone_t)(thread_t* t, struct THREAD_MAPPING* tdest, struct THREAD_MAPPING* tsrc);

/* Destroy function: cleans up the given mapping's private data */
typedef errorcode_t (*threadmap_destroy_t)(thread_t* t, struct THREAD_MAPPING* tm);

struct THREAD_MAPPING {
	unsigned int		tm_flags;		/* flags */
	addr_t			tm_virt;		/* userland address */
	size_t			tm_len;			/* length */
	addr_t			tm_phys;		/* physical address */
	void*			tm_privdata;		/* private data */
	threadmap_fault_t	tm_fault;		/* fault function */
	threadmap_clone_t	tm_clone;		/* clone function */
	threadmap_destroy_t	tm_destroy;		/* destroy function */

	DQUEUE_FIELDS(struct THREAD_MAPPING);
};

DQUEUE_DEFINE(THREAD_MAPPING_QUEUE, struct THREAD_MAPPING);

struct THREAD {
	/* Machine-dependant data - must be first */
	MD_THREAD_FIELDS

	spinlock_t t_lock;	/* Lock protecting the thread data */

	unsigned int t_flags;
#define THREAD_FLAG_ACTIVE	0x0001	/* Thread is scheduled somewhere */
#define THREAD_FLAG_SUSPENDED	0x0002	/* Thread is currently suspended */
#define THREAD_FLAG_TERMINATING	0x0004	/* Thread is terminating */
#define THREAD_FLAG_ZOMBIE	0x0008	/* Thread has no more resources */
#define THREAD_FLAG_RESCHEDULE	0x0010	/* Thread desires a reschedule */
#define THREAD_FLAG_KTHREAD	0x8000	/* Kernel thread */

	unsigned int t_terminate_info;
#define THREAD_MAKE_EXITCODE(a,b) (((a) << 24) | ((b) & 0x00ffffff))
#define THREAD_TERM_SYSCALL	0x1	/* euthanasia */
#define THREAD_TERM_FAULT	0x2	/* programming fault */
#define THREAD_TERM_FAILURE	0x3	/* generic failure */

	addr_t t_next_mapping;		/* address of next mapping */
	struct THREAD_MAPPING_QUEUE t_mappings;

	int t_priority;			/* priority (0 highest) */
#define THREAD_PRIORITY_DEFAULT	200
#define THREAD_PRIORITY_IDLE	255
	int t_affinity;			/* thread CPU */
#define THREAD_AFFINITY_ANY -1

	struct THREADINFO* t_threadinfo;	/* Thread startup information */

	/* Thread handles */
	struct HANDLE* t_thread_handle;	/* Handle identifying this thread */
	struct HANDLE_QUEUE t_handles;	/* Handles owned by the thread */
	struct HANDLE* t_path_handle;	/* Current path */

	/* Scheduler specific information */
	struct SCHED_PRIV t_sched_priv;

	DQUEUE_FIELDS(thread_t);
};

DQUEUE_DEFINE(THREAD_QUEUE, thread_t);

/* Macro's to facilitate flag checking */
#define THREAD_IS_ACTIVE(t) ((t)->t_flags & THREAD_FLAG_ACTIVE)
#define THREAD_IS_SUSPENDED(t) ((t)->t_flags & THREAD_FLAG_SUSPENDED)
#define THREAD_IS_TERMINATING(t) ((t)->t_flags & THREAD_FLAG_TERMINATING)
#define THREAD_IS_ZOMBIE(t) ((t)->t_flags & THREAD_FLAG_ZOMBIE)
#define THREAD_WANT_RESCHEDULE(t) ((t)->t_flags & THREAD_FLAG_RESCHEDULE)
#define THREAD_IS_KTHREAD(t) ((t)->t_flags & THREAD_FLAG_KTHREAD)

/* Machine-dependant callback to initialize a thread */
errorcode_t md_thread_init(thread_t* thread);
errorcode_t md_kthread_init(thread_t* thread, kthread_func_t func, void* arg);

/* Machine-dependant callback to free thread data */
void md_thread_free(thread_t* thread);

errorcode_t thread_init(thread_t* t, thread_t* parent);
errorcode_t kthread_init(thread_t* t, kthread_func_t func, void* arg);
errorcode_t thread_alloc(thread_t* parent, thread_t** dest);
void thread_free(thread_t* t);
void thread_destroy(thread_t* t);
errorcode_t thread_set_args(thread_t* t, const char* args, size_t args_len);
errorcode_t thread_set_environment(thread_t* t, const char* env, size_t env_len);

void md_thread_switch(thread_t* new, thread_t* old);
void idle_thread();

/* Thread memory map flags */
#define THREAD_MAP_READ 	0x01	/* Read */
#define THREAD_MAP_WRITE	0x02	/* Write */
#define THREAD_MAP_EXECUTE	0x04	/* Execute */
#define THREAD_MAP_LAZY		0x08	/* Lazy mapping: page in as needed */
#define THREAD_MAP_ALLOC 	0x10	/* Allocate memory for mapping */
#define THREAD_MAP_PRIVATE 	0x20	/* Private mapping, will not be cloned */
#define THREAD_MAP_DEVICE	0x40	/* Device memory */
void md_thread_set_entrypoint(thread_t* thread, addr_t entry);
void md_thread_set_argument(thread_t* thread, addr_t arg);
void* md_thread_map(thread_t* thread, void* to, void* from, size_t length, int flags);
errorcode_t thread_unmap(thread_t* t, addr_t virt, size_t len);
void* md_map_thread_memory(thread_t* thread, void* ptr, size_t length, int write);
void md_thread_clone(thread_t* t, thread_t* parent, register_t retval);
errorcode_t md_thread_unmap(thread_t* thread, addr_t virt, size_t length);
int md_thread_peek_32(thread_t* thread, addr_t virt, uint32_t* val);
addr_t md_thread_is_mapped(thread_t* thread, addr_t virt, int flags);

errorcode_t thread_mapto(thread_t* t, addr_t virt, addr_t phys, size_t len, uint32_t flags, struct THREAD_MAPPING** out);
errorcode_t thread_map(thread_t* t, addr_t from, size_t len, uint32_t flags, struct THREAD_MAPPING** out);
void thread_free_mapping(thread_t* t, struct THREAD_MAPPING* tm);
void thread_free_mappings(thread_t* t);
errorcode_t thread_handle_fault(thread_t* t, addr_t virt, int flags);

void thread_suspend(thread_t* t);
void thread_resume(thread_t* t);
void thread_exit(int exitcode);
void thread_dump(int num_args, char** arg);
errorcode_t thread_clone(thread_t* parent, int flags, thread_t** dest);

/*
 * Thread callback functions are provided so that modules can take action upon
 * creating or destroying of threads.
 */
typedef errorcode_t (*thread_callback_t)(thread_t* thread, thread_t* parent);
struct THREAD_CALLBACK {
	thread_callback_t tc_func;
	DQUEUE_FIELDS(struct THREAD_CALLBACK);
};
DQUEUE_DEFINE(THREAD_CALLBACKS, struct THREAD_CALLBACK);

errorcode_t thread_register_init_func(struct THREAD_CALLBACK* fn);
errorcode_t thread_register_exit_func(struct THREAD_CALLBACK* fn);
errorcode_t thread_unregister_init_func(struct THREAD_CALLBACK* fn);
errorcode_t thread_unregister_exit_func(struct THREAD_CALLBACK* fn);

#define REGISTER_THREAD_INIT_FUNC(fn) \
	static struct THREAD_CALLBACK cb_init_##fn = { .tc_func = fn }; \
	static errorcode_t register_##fn() { \
		return thread_register_init_func(&cb_init_##fn); \
	} \
	static errorcode_t unregister_##fn() { \
		return thread_unregister_init_func(&cb_init_##fn); \
	} \
	INIT_FUNCTION(register_##fn, SUBSYSTEM_THREAD, ORDER_FIRST); \
	EXIT_FUNCTION(unregister_##fn)

#define REGISTER_THREAD_EXIT_FUNC(fn) \
	static struct THREAD_CALLBACK cb_init_##fn = { .tc_func = fn }; \
	static errorcode_t register_##fn() { \
		return thread_register_exit_func(&cb_exit_##fn); \
	} \
	static errorcode_t unregister_##fn() { \
		return thread_unregister_exit_func(&cb_exit_##fn); \
	} \
	INIT_FUNCTION(register_##fn, SUBSYSTEM_THREAD, ORDER_FIRST); \
	EXIT_FUNCTION(unregister_##fn)

#endif
