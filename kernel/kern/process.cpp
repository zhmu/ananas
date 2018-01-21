#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/procinfo.h>
#include "kernel/handle.h"
#include "kernel/init.h"
#include "kernel/kdb.h"
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/process.h"
#include "kernel/trace.h"
#include "kernel/vm.h"
#include "kernel/vmspace.h"
#include "options.h"

TRACE_SETUP;

/* XXX These should be locked */

static process::CallbackList process_callbacks_init;
static process::CallbackList process_callbacks_exit;

namespace process {

mutex_t process_mtx;
ProcessList process_all;

namespace {

semaphore_t process_sleep_sem;
pid_t process_curpid = -1;

pid_t AllocateProcessID()
{
	/* XXX this is a bit of a kludge for now ... */
	mutex_lock(&process_mtx);
	pid_t pid = process_curpid++;
	mutex_unlock(&process_mtx);

	return pid;
}


} // unnamed namespace

} // namespace process

static errorcode_t
process_alloc_ex(Process* parent, Process*& dest, int flags)
{
	errorcode_t err;

	auto p = new Process;
	memset(p, 0, sizeof(*p));
	p->p_parent = parent; /* XXX should we take a ref here? */
	p->p_refcount = 1; /* caller */
	p->p_state = PROCESS_STATE_ACTIVE;
	p->p_pid = process::AllocateProcessID();
	mutex_init(&p->p_lock, "plock");

	/* Create the process's vmspace */
	err = vmspace_create(p->p_vmspace);
	if (ananas_is_failure(err)) {
		kfree(p);
		return err;
	}
	VMSpace& vs = *p->p_vmspace;

	// Map a process info structure so everything beloning to this process can use it
	VMArea* va;
	err = vmspace_map(vs, sizeof(struct PROCINFO), VM_FLAG_USER | VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_NO_CLONE, va);
	if (ananas_is_failure(err))
		goto fail;
	p->p_info_va = va->va_virt;

	// Now hook the process info structure up to it
	{
		// XXX we should have a separate vmpage_create_...() for this that sets vp_vaddr
		VMPage& vp = vmpage_create_private(va, 0);
		vp.vp_vaddr = va->va_virt;
		p->p_info = static_cast<struct PROCINFO*>(kmem_map(page_get_paddr(*vmpage_get_page(vp)), sizeof(struct PROCINFO), VM_FLAG_READ | VM_FLAG_WRITE));
		vmpage_map(vs, *va, vp);
		vmpage_unlock(vp);
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
			err = handle_clone(*parent, n, nullptr, *p, &handle, n, &out);
			if (ananas_is_failure(err))
				goto fail;
			KASSERT(n == out, "cloned handle %d to new handle %d", n, out);
		}
	}
	/* Run all process initialization callbacks */
	for(auto& pc: process_callbacks_init) {
		err = pc.pc_func(*p);
		if (ananas_is_failure(err))
			goto fail;
	}

	/* Grab the parent's lock and insert the child */
	if (parent != nullptr) {
		process_lock(*parent);
		parent->p_children.push_back(*p);
		process_unlock(*parent);
	}

	/* Finally, add the process to all processes */
	mutex_lock(&process::process_mtx);
	process::process_all.push_back(*p);
	mutex_unlock(&process::process_mtx);

	dest = p;
	return ananas_success();

fail:
	vmspace_destroy(*p->p_vmspace);
	kfree(p);
	return err;
}

errorcode_t
process_alloc(Process* parent, Process*& dest)
{
	return process_alloc_ex(parent, dest, 0);
}

errorcode_t
process_clone(Process& p, int flags, Process*& out_p)
{
	errorcode_t err;
	Process* newp;
	err = process_alloc_ex(&p, newp, 0);
	ANANAS_ERROR_RETURN(err);

	/* Duplicate the vmspace - this should leave the private mappings alone */
	err = vmspace_clone(*p.p_vmspace, *newp->p_vmspace, 0);
	if (ananas_is_failure(err)) {
		process_deref(*newp);
		return err;
	}

	out_p = newp;
	return ananas_success();
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

	/* Clean the process's vmspace up - this will remove all non-essential mappings */
	vmspace_cleanup(*p.p_vmspace);

	/* Remove the process from the all-process list */
	mutex_lock(&process::process_mtx);
	process::process_all.remove(p);
	mutex_unlock(&process::process_mtx);

	/*
	 * Unmap the process information; no one can query it at this point as the
	 * process itself will not run anymore.
	 */
	kmem_unmap(p.p_info, sizeof(struct PROCINFO));
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
	process_lock(p);
	p.p_state = PROCESS_STATE_ZOMBIE;
	p.p_exit_status = status;
	process_unlock(p);

	sem_signal(&process::process_sleep_sem);
}

errorcode_t
process_wait_and_lock(Process& parent, int flags, Process*& p_out)
{
	if (flags != 0)
		return ANANAS_ERROR(BAD_FLAG);
	/*
	 * XXX We aren't going for efficiency here - thus we use a single
	 *     semaphore to wake anything up once a process has exited.
	 */
	for(;;) {
		process_lock(parent);
		for(auto& child: parent.p_children) {
			process_lock(child);
			if (child.p_state == PROCESS_STATE_ZOMBIE) {
				/* Found one; remove it from the parent's list */
				parent.p_children.remove(child);
				process_unlock(parent);

				/* Note that we give our ref to the caller! */
				p_out = &child;
				return ananas_success();
			}
			process_unlock(child);
		}
		process_unlock(parent);

		/* Nothing good yet; sleep on it */
		sem_wait(&process::process_sleep_sem);
	}

	/* NOTREACHED */
}

errorcode_t
process_set_args(Process& p, const char* args, size_t args_len)
{
	if (args_len >= (PROCINFO_ARGS_LENGTH - 1))
		args_len = PROCINFO_ARGS_LENGTH - 1;
	for (unsigned int i = 0; i < args_len; i++)
		if(args[i] == '\0' && args[i + 1] == '\0') {
			memcpy(p.p_info->pi_args, args, i + 2 /* terminating \0\0 */);
			return ananas_success();
		}
	return ANANAS_ERROR(BAD_LENGTH);
}

errorcode_t
process_set_environment(Process& p, const char* env, size_t env_len)
{
	if (env_len >= (PROCINFO_ENV_LENGTH - 1))
		env_len = PROCINFO_ENV_LENGTH - 1;
	for (unsigned int i = 0; i < env_len; i++)
		if(env[i] == '\0' && env[i + 1] == '\0') {
			memcpy(p.p_info->pi_env, env, i + 2 /* terminating \0\0 */);
			return ananas_success();
		}

	return ANANAS_ERROR(BAD_LENGTH);
}

Process*
process_lookup_by_id_and_ref(pid_t pid)
{
	mutex_lock(&process::process_mtx);
	for(auto& p: process::process_all) {
		process_lock(p);
		if (p.p_pid != pid) {
			process_unlock(p);
			continue;
		}

		// Process found; get a ref and return it
		process_ref(p);
		process_unlock(p);
		mutex_unlock(&process::process_mtx);
		return &p;
	}
	mutex_unlock(&process::process_mtx);
	return nullptr;
}

errorcode_t
process_register_init_func(process::Callback& fn)
{
	process_callbacks_init.push_back(fn);
	return ananas_success();
}

errorcode_t
process_register_exit_func(process::Callback& fn)
{
	process_callbacks_exit.push_back(fn);
	return ananas_success();
}

errorcode_t
process_unregister_init_func(process::Callback& fn)
{
	process_callbacks_init.remove(fn);
	return ananas_success();
}

errorcode_t
process_unregister_exit_func(process::Callback& fn)
{
	process_callbacks_exit.remove(fn);
	return ananas_success();
}

static errorcode_t
process_init()
{
	mutex_init(&process::process_mtx, "proc");
	sem_init(&process::process_sleep_sem, 0);
	process::process_curpid = 1;

	return ananas_success();
}

INIT_FUNCTION(process_init, SUBSYSTEM_PROCESS, ORDER_FIRST);

#ifdef OPTION_KDB

void vmspace_dump(VMSpace&);

KDB_COMMAND(ps, "[s:flags]", "Displays all processes")
{
	mutex_lock(&process::process_mtx);
	for(auto& p: process::process_all) {
		kprintf("process %d (%p): state %d\n", p.p_pid, &p, p.p_state);
		vmspace_dump(*p.p_vmspace);
	}
	mutex_unlock(&process::process_mtx);
}

#endif // OPTION_KDB

/* vim:set ts=2 sw=2: */
