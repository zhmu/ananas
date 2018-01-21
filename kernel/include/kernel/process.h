#ifndef __PROCESS_H__
#define __PROCESS_H__

#include <ananas/limits.h>
#include <ananas/util/list.h>
#include "kernel/lock.h"

struct DEntry;
struct PROCINFO;

struct Thread;
class VMSpace;

#define PROCESS_STATE_ACTIVE	1
#define PROCESS_STATE_ZOMBIE	2

struct Process;

namespace process {

// Internal stuff so we can work with children and all nodes
namespace internal {

template<typename T>
struct AllNode
{
	static typename util::List<T>::Node& Get(T& t) {
		return t.p_NodeAll;
	}
};

template<typename T>
struct ChildrenNode
{
	static typename util::List<T>::Node& Get(T& t) {
		return t.p_NodeChildren;
	}
};

template<typename T> using ProcessAllNodeAccessor = typename util::List<T>::template nodeptr_accessor<AllNode<T> >;
template<typename T> using ProcessChildrenNodeAccessor = typename util::List<T>::template nodeptr_accessor<ChildrenNode<T> >;

}

typedef util::List<::Process, internal::ProcessAllNodeAccessor<::Process> > ChildrenList;
typedef util::List<::Process, internal::ProcessChildrenNodeAccessor<::Process> > ProcessList;

// Callbacks so we can organise things when needed
typedef errorcode_t (*process_callback_t)(Process& proc);
struct Callback : util::List<Callback>::NodePtr {
	Callback(process_callback_t func) : pc_func(func) { }
	process_callback_t pc_func;
};
typedef util::List<Callback> CallbackList;

} // namespace process


struct Process {
	mutex_t p_lock;	/* Locks the process */

	unsigned int p_state;		/* Process state */
	refcount_t p_refcount;		/* Reference count of the process, >0 */

	pid_t	p_pid;	/* Process ID */
	int	p_exit_status;		/* Exit status / code */

	Process* 	p_parent;	/* Parent process, if any */
	VMSpace*	p_vmspace;	/* Process memory space */

	struct PROCINFO* p_info;	/* Process startup information */
	addr_t p_info_va;

	Thread* p_mainthread;		/* Main thread */

	struct HANDLE* p_handle[PROCESS_MAX_HANDLES];	/* Handles */

	DEntry*	p_cwd;		/* Current path */

	process::ChildrenList	p_children;	// List of this process' children

	util::List<Process>::Node p_NodeAll;
	util::List<Process>::Node p_NodeChildren;
};

static inline void process_lock(Process& p)
{
	mutex_lock(&p.p_lock);
}

static inline void process_unlock(Process& p)
{
	mutex_unlock(&p.p_lock);
}

errorcode_t process_alloc(Process* parent, Process*& dest);

void process_ref(Process& p);
void process_deref(Process& p);
void process_exit(Process& p, int status);
errorcode_t process_set_args(Process& p, const char* args, size_t args_len);
errorcode_t process_set_environment(Process& p, const char* env, size_t env_len);
errorcode_t process_clone(Process& p, int flags, Process*& out_p);
errorcode_t process_wait_and_lock(Process& p, int flags, Process*& p_out);
Process* process_lookup_by_id_and_ref(pid_t pid);

/*
 * Process callback functions are provided so that modules can take action upon
 * creating or destroying of processes.
 */

errorcode_t process_register_init_func(process::Callback& pc);
errorcode_t process_register_exit_func(process::Callback& pc);
errorcode_t process_unregister_init_func(process::Callback& pc);
errorcode_t process_unregister_exit_func(process::Callback& pc);

#define REGISTER_PROCESS_INIT_FUNC(fn) \
	static process::Callback cb_init_##fn(fn); \
	static errorcode_t register_##fn() { \
		return process_register_init_func(cb_init_##fn); \
	} \
	static errorcode_t unregister_##fn() { \
		return process_unregister_init_func(cb_init_##fn); \
	} \
	INIT_FUNCTION(register_##fn, SUBSYSTEM_THREAD, ORDER_FIRST); \
	EXIT_FUNCTION(unregister_##fn)

#define REGISTER_PROCESS_EXIT_FUNC(fn) \
	static process::Callback cb_exit_##fn(fn); \
	static errorcode_t register_##fn() { \
		return process_register_exit_func(cb_exit_##fn); \
	} \
	static errorcode_t unregister_##fn() { \
		return process_unregister_exit_func(cb_exit_##fn); \
	} \
	INIT_FUNCTION(register_##fn, SUBSYSTEM_THREAD, ORDER_FIRST); \
	EXIT_FUNCTION(unregister_##fn)

#endif /* __PROCESS_H__ */
