#ifndef __PROCESS_H__
#define __PROCESS_H__

#include <ananas/limits.h>
#include <ananas/util/list.h>
#include "kernel/lock.h"

struct DEntry;
struct PROCINFO;
class Result;

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
typedef Result (*process_callback_t)(Process& proc);
struct Callback : util::List<Callback>::NodePtr {
	Callback(process_callback_t func) : pc_func(func) { }
	process_callback_t pc_func;
};
typedef util::List<Callback> CallbackList;

} // namespace process


struct Process {
	void Lock()
	{
		p_lock.Lock();
	}

	void Unlock()
	{
		p_lock.Unlock();
	}

	unsigned int p_state = 0;	/* Process state */
	refcount_t p_refcount = 0;		/* Reference count of the process, >0 */

	pid_t	p_pid = 0;/* Process ID */
	int	p_exit_status = 0;	/* Exit status / code */

	Process* 	p_parent = nullptr;	/* Parent process, if any */
	VMSpace*	p_vmspace = nullptr;	/* Process memory space */

	struct PROCINFO* p_info = nullptr; 	/* Process startup information */
	addr_t p_info_va = 0;

	Thread* p_mainthread = nullptr;		/* Main thread */

	struct HANDLE* p_handle[PROCESS_MAX_HANDLES] = {0};	/* Handles */

	DEntry*	p_cwd = nullptr;		/* Current path */

	process::ChildrenList	p_children;	// List of this process' children

	util::List<Process>::Node p_NodeAll;
	util::List<Process>::Node p_NodeChildren;

private:
	Mutex p_lock{"plock"};	/* Locks the process */
};

Result process_alloc(Process* parent, Process*& dest);

void process_ref(Process& p);
void process_deref(Process& p);
void process_exit(Process& p, int status);
Result process_set_args(Process& p, const char* args, size_t args_len);
Result process_set_environment(Process& p, const char* env, size_t env_len);
Result process_clone(Process& p, int flags, Process*& out_p);
Result process_wait_and_lock(Process& p, int flags, Process*& p_out);
Process* process_lookup_by_id_and_ref(pid_t pid);

/*
 * Process callback functions are provided so that modules can take action upon
 * creating or destroying of processes.
 */

Result process_register_init_func(process::Callback& pc);
Result process_register_exit_func(process::Callback& pc);
Result process_unregister_init_func(process::Callback& pc);
Result process_unregister_exit_func(process::Callback& pc);

#define REGISTER_PROCESS_INIT_FUNC(fn) \
	static process::Callback cb_init_##fn(fn); \
	static Result register_##fn() { \
		return process_register_init_func(cb_init_##fn); \
	} \
	static Result unregister_##fn() { \
		return process_unregister_init_func(cb_init_##fn); \
	} \
	INIT_FUNCTION(register_##fn, SUBSYSTEM_THREAD, ORDER_FIRST); \
	EXIT_FUNCTION(unregister_##fn)

#define REGISTER_PROCESS_EXIT_FUNC(fn) \
	static process::Callback cb_exit_##fn(fn); \
	static Result register_##fn() { \
		return process_register_exit_func(cb_exit_##fn); \
	} \
	static Result unregister_##fn() { \
		return process_unregister_exit_func(cb_exit_##fn); \
	} \
	INIT_FUNCTION(register_##fn, SUBSYSTEM_THREAD, ORDER_FIRST); \
	EXIT_FUNCTION(unregister_##fn)

#endif /* __PROCESS_H__ */
