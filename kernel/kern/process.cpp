#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/procinfo.h>
#include "kernel/handle.h"
#include "kernel/init.h"
#include "kernel/kdb.h"
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/process.h"
#include "kernel/processgroup.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vm.h"
#include "kernel/vmspace.h"
#include "options.h"

TRACE_SETUP;

/* XXX These should be locked */

static process::CallbackList process_callbacks_init;
static process::CallbackList process_callbacks_exit;

namespace process {

Mutex process_mtx("process");
ProcessList process_all;

namespace {

Semaphore process_sleep_sem(0);
pid_t process_curpid = -1;

pid_t AllocateProcessID()
{
	/* XXX this is a bit of a kludge for now ... */
	MutexGuard g(process_mtx);
	return process_curpid++;
}


} // unnamed namespace

} // namespace process

static Result
process_alloc_ex(Process* parent, Process*& dest, int flags)
{
	auto p = new Process;
	p->p_parent = parent; /* XXX should we take a ref here? */
	p->p_refcount = 1; /* caller */
	p->p_state = PROCESS_STATE_ACTIVE;
	p->p_pid = process::AllocateProcessID();

	/* Create the process's vmspace */
	if (auto result = vmspace_create(p->p_vmspace); result.IsFailure()) {
		delete p;
		return result;
	}
	VMSpace& vs = *p->p_vmspace;

	// Map a process info structure so everything beloning to this process can use it
	VMArea* va;
	auto result = vs.Map(sizeof(struct PROCINFO), VM_FLAG_USER | VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_NO_CLONE, va);
	if (result.IsFailure())
		goto fail;
	p->p_info_va = va->va_virt;

	// Now hook the process info structure up to it
	{
		// XXX we should have a separate vmpage_create_...() for this that sets vp_vaddr
		VMPage& vp = vmpage_create_private(va, 0);
		vp.vp_vaddr = va->va_virt;
		p->p_info = static_cast<struct PROCINFO*>(kmem_map(vp.GetPage()->GetPhysicalAddress(), sizeof(struct PROCINFO), VM_FLAG_READ | VM_FLAG_WRITE));
		vmpage_map(vs, *va, vp);
		vp.Unlock();
	}

	/* Initialize process information structure */
	memset(p->p_info, 0, sizeof(struct PROCINFO));
	p->p_info->pi_size = sizeof(struct PROCINFO);
	p->p_info->pi_pid = p->p_pid;
	if (parent != nullptr)
		process_set_environment(*p, parent->p_info->pi_env, PROCINFO_ENV_LENGTH - 1);

	// Clone the parent's handles
	if (parent != nullptr) {
		for (unsigned int n = 0; n < PROCESS_MAX_HANDLES; n++) {
			if (parent->p_handle[n] == nullptr)
				continue;

			struct HANDLE* handle;
			handleindex_t out;
			result = handle_clone(*parent, n, nullptr, *p, &handle, n, &out);
			if (result.IsFailure())
				goto fail;
			KASSERT(n == out, "cloned handle %d to new handle %d", n, out);
		}
	}
	/* Run all process initialization callbacks */
	for(auto& pc: process_callbacks_init) {
		result = pc.pc_func(*p);
		if (result.IsFailure())
			goto fail;
	}

	/* Grab the parent's lock and insert the child */
	if (parent != nullptr) {
		parent->Lock();
		parent->p_children.push_back(*p);
		parent->Unlock();
	}

	process::InitializeProcessGroup(*p, parent);

	/* Finally, add the process to all processes */
	{
		MutexGuard g(process::process_mtx);
		process::process_all.push_back(*p);
	}

	dest = p;
	return Result::Success();

fail:
	vmspace_destroy(*p->p_vmspace);
	delete p;
	return result;
}

Result
process_alloc(Process* parent, Process*& dest)
{
	return process_alloc_ex(parent, dest, 0);
}

Result
process_clone(Process& p, int flags, Process*& out_p)
{
	Process* newp;
	RESULT_PROPAGATE_FAILURE(
		process_alloc_ex(&p, newp, 0)
	);

	/* Duplicate the vmspace - this should leave the private mappings alone */
	if (auto result = p.p_vmspace->Clone(*newp->p_vmspace, 0); result.IsFailure()) {
		process_deref(*newp);
		return result;
	}

	out_p = newp;
	return Result::Success();
}

static void
process_destroy(Process& p)
{
	/* Run all process exit callbacks */
	for(auto& pc: process_callbacks_exit) {
		pc.pc_func(p);
	}

	/* Free all handles */
	for(unsigned int n = 0; n < PROCESS_MAX_HANDLES; n++)
		handle_free_byindex(p, n);

	process::AbandonProcessGroup(p);

	// Process is gone - destroy any memory mappings held by it
	vmspace_destroy(*p.p_vmspace);

	/* Remove the process from the all-process list */
	{
		MutexGuard g(process::process_mtx);
		process::process_all.remove(p);
	}
}

void
process_ref(Process& p)
{
	KASSERT(p.p_refcount > 0, "reffing process with invalid refcount %d", p.p_refcount);
	++p.p_refcount;
}

void
process_deref(Process& p)
{
	KASSERT(p.p_refcount > 0, "dereffing process with invalid refcount %d", p.p_refcount);

	if (--p.p_refcount == 0)
		process_destroy(p);
}

void
process_exit(Process& p, int status)
{
	p.Lock();
	p.p_state = PROCESS_STATE_ZOMBIE;
	p.p_exit_status = status;
	p.Unlock();

	process::process_sleep_sem.Signal();
}

Result
process_wait_and_lock(Process& parent, int flags, Process*& p_out)
{
	if (flags != 0)
		return RESULT_MAKE_FAILURE(EINVAL);
	/*
	 * XXX We aren't going for efficiency here - thus we use a single
	 *     semaphore to wake anything up once a process has exited.
	 */
	for(;;) {
		parent.Lock();
		for(auto& child: parent.p_children) {
			child.Lock();
			if (child.p_state == PROCESS_STATE_ZOMBIE) {
				// Found one; remove it from the parent's list
				parent.p_children.remove(child);
				parent.Unlock();

				/*
				 * Deref the main thread; this should kill it as there is nothing else
				 * left.
				 */
				KASSERT(child.p_mainthread != nullptr, "zombie child without main thread?");
				KASSERT(child.p_mainthread->t_refcount == 1, "zombie child main thread still has %d refs", child.p_mainthread->t_refcount);
				child.p_mainthread->Deref();

				/* Note that we give our ref to the caller! */
				p_out = &child;
				return Result::Success();
			}
			child.Unlock();
		}
		parent.Unlock();

		/* Nothing good yet; sleep on it */
		process::process_sleep_sem.Wait();
	}

	/* NOTREACHED */
}

Result
process_set_args(Process& p, const char* args, size_t args_len)
{
	if (args_len >= (PROCINFO_ARGS_LENGTH - 1))
		args_len = PROCINFO_ARGS_LENGTH - 1;
	for (unsigned int i = 0; i < args_len; i++)
		if(args[i] == '\0' && args[i + 1] == '\0') {
			memcpy(p.p_info->pi_args, args, i + 2 /* terminating \0\0 */);
			return Result::Success();
		}
	return RESULT_MAKE_FAILURE(EINVAL);
}

Result
process_set_environment(Process& p, const char* env, size_t env_len)
{
	if (env_len >= (PROCINFO_ENV_LENGTH - 1))
		env_len = PROCINFO_ENV_LENGTH - 1;
	for (unsigned int i = 0; i < env_len; i++)
		if(env[i] == '\0' && env[i + 1] == '\0') {
			memcpy(p.p_info->pi_env, env, i + 2 /* terminating \0\0 */);
			return Result::Success();
		}

	return RESULT_MAKE_FAILURE(EINVAL);
}

Process*
process_lookup_by_id_and_ref(pid_t pid)
{
	MutexGuard g(process::process_mtx);

	for(auto& p: process::process_all) {
		p.Lock();
		if (p.p_pid != pid) {
			p.Unlock();
			continue;
		}

		// Process found; get a ref and return it
		process_ref(p);
		p.Unlock();
		return &p;
	}
	return nullptr;
}

Result
process_register_init_func(process::Callback& fn)
{
	process_callbacks_init.push_back(fn);
	return Result::Success();
}

Result
process_register_exit_func(process::Callback& fn)
{
	process_callbacks_exit.push_back(fn);
	return Result::Success();
}

Result
process_unregister_init_func(process::Callback& fn)
{
	process_callbacks_init.remove(fn);
	return Result::Success();
}

Result
process_unregister_exit_func(process::Callback& fn)
{
	process_callbacks_exit.remove(fn);
	return Result::Success();
}

static Result
process_init()
{
	process::process_curpid = 1;

	return Result::Success();
}

INIT_FUNCTION(process_init, SUBSYSTEM_PROCESS, ORDER_FIRST);

#ifdef OPTION_KDB

KDB_COMMAND(ps, "[s:flags]", "Displays all processes")
{
	MutexGuard g(process::process_mtx);
	for(auto& p: process::process_all) {
		kprintf("process %d (%p): state %d\n", p.p_pid, &p, p.p_state);
		p.p_vmspace->Dump();
	}
}

#endif // OPTION_KDB

/* vim:set ts=2 sw=2: */
