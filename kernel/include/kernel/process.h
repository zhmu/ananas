#ifndef __PROCESS_H__
#define __PROCESS_H__

#include <ananas/limits.h>
#include "kernel/list.h"
#include "kernel/lock.h"

struct DENTRY;
struct PROCINFO;
class VMSpace;

#define PROCESS_STATE_ACTIVE	1
#define PROCESS_STATE_ZOMBIE	2

struct PROCESS;
LIST_DEFINE(PROCESS_QUEUE, struct PROCESS);

struct PROCESS {
	mutex_t p_lock;	/* Locks the process */

	unsigned int p_state;		/* Process state */
	refcount_t p_refcount;		/* Reference count of the process, >0 */

	pid_t	p_pid;	/* Process ID */
	int	p_exit_status;		/* Exit status / code */

	struct PROCESS* p_parent;	/* Parent process, if any */
	VMSpace*	p_vmspace;	/* Process memory space */

	struct PROCINFO* p_info;	/* Process startup information */
	addr_t p_info_va;

	thread_t* p_mainthread;		/* Main thread */

	struct HANDLE* p_handle[PROCESS_MAX_HANDLES];	/* Handles */

	struct DENTRY* p_cwd;		/* Current path */

	struct PROCESS_QUEUE	p_children;	/* Queue of this process' children */

        LIST_FIELDS_IT(struct PROCESS, all);
        LIST_FIELDS_IT(struct PROCESS, children);
};

static inline void process_lock(process_t* p)
{
	mutex_lock(&p->p_lock);
}

static inline void process_unlock(process_t* p)
{
	mutex_unlock(&p->p_lock);
}

errorcode_t process_alloc(process_t* parent, process_t** dest);

void process_ref(process_t* p);
void process_deref(process_t* p);
void process_exit(process_t* p, int status);
errorcode_t process_set_args(process_t* p, const char* args, size_t args_len);
errorcode_t process_set_environment(process_t* p, const char* env, size_t env_len);
errorcode_t process_clone(process_t* p, int flags, process_t** out_p);
errorcode_t process_wait_and_lock(process_t* p, int flags, process_t** p_out);
process_t* process_lookup_by_id_and_ref(pid_t pid);

/*
 * Process callback functions are provided so that modules can take action upon
 * creating or destroying of processes.
 */
typedef errorcode_t (*process_callback_t)(process_t* proc);
struct PROCESS_CALLBACK {
	process_callback_t pc_func;
	LIST_FIELDS(struct PROCESS_CALLBACK);
};
LIST_DEFINE(PROCESS_CALLBACKS, struct PROCESS_CALLBACK);

errorcode_t process_register_init_func(struct PROCESS_CALLBACK* pc);
errorcode_t process_register_exit_func(struct PROCESS_CALLBACK* pc);
errorcode_t process_unregister_init_func(struct PROCESS_CALLBACK* pc);
errorcode_t process_unregister_exit_func(struct PROCESS_CALLBACK* pc);

#define REGISTER_PROCESS_INIT_FUNC(fn) \
	static struct PROCESS_CALLBACK cb_init_##fn = { .pc_func = fn }; \
	static errorcode_t register_##fn() { \
		return process_register_init_func(&cb_init_##fn); \
	} \
	static errorcode_t unregister_##fn() { \
		return process_unregister_init_func(&cb_init_##fn); \
	} \
	INIT_FUNCTION(register_##fn, SUBSYSTEM_THREAD, ORDER_FIRST); \
	EXIT_FUNCTION(unregister_##fn)

#define REGISTER_PROCESS_EXIT_FUNC(fn) \
	static struct PROCESS_CALLBACK cb_exit_##fn = { .pc_func = fn }; \
	static errorcode_t register_##fn() { \
		return process_register_exit_func(&cb_exit_##fn); \
	} \
	static errorcode_t unregister_##fn() { \
		return process_unregister_exit_func(&cb_exit_##fn); \
	} \
	INIT_FUNCTION(register_##fn, SUBSYSTEM_THREAD, ORDER_FIRST); \
	EXIT_FUNCTION(unregister_##fn)

#endif /* __PROCESS_H__ */
