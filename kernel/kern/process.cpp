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
static struct PROCESS_CALLBACKS process_callbacks_init;
static struct PROCESS_CALLBACKS process_callbacks_exit;

namespace Ananas {
namespace Process {

mutex_t process_mtx;
struct PROCESS_QUEUE process_all;

namespace {
semaphore_t process_sleep_sem;
pid_t process_curpid = -1;
} // unnamed namespace

} // namespace Process
} // namespace Ananas

static pid_t
process_alloc_pid()
{
	/* XXX this is a bit of a kludge for now ... */
	mutex_lock(&Ananas::Process::process_mtx);
	pid_t pid = Ananas::Process::process_curpid++;
	mutex_unlock(&Ananas::Process::process_mtx);

	return pid;
}

static errorcode_t
process_alloc_ex(process_t* parent, process_t** dest, int flags)
{
	errorcode_t err;

	auto p = new PROCESS;
	memset(p, 0, sizeof(*p));
	p->p_parent = parent; /* XXX should we take a ref here? */
	p->p_refcount = 1; /* caller */
	p->p_state = PROCESS_STATE_ACTIVE;
	p->p_pid = process_alloc_pid();
	mutex_init(&p->p_lock, "plock");
	LIST_INIT(&p->p_children);

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
		p->p_info = static_cast<struct PROCINFO*>(kmem_map(page_get_paddr(vmpage_get_page(vp)), sizeof(struct PROCINFO), VM_FLAG_READ | VM_FLAG_WRITE));
		vmpage_map(vs, *va, vp);
		vmpage_unlock(vp);
	}

	/* Initialize process information structure */
	memset(p->p_info, 0, sizeof(struct PROCINFO));
	p->p_info->pi_size = sizeof(struct PROCINFO);
	p->p_info->pi_pid = p->p_pid;
	if (parent != NULL)
		process_set_environment(p, parent->p_info->pi_env, PROCINFO_ENV_LENGTH - 1);

	// Clone the parent's handles
	if (parent != NULL) {
		for (unsigned int n = 0; n < PROCESS_MAX_HANDLES; n++) {
			if (parent->p_handle[n] == NULL)
				continue;

			struct HANDLE* handle;
			handleindex_t out;
			err = handle_clone(parent, n, NULL, p, &handle, n, &out);
			if (ananas_is_failure(err))
				goto fail;
			KASSERT(n == out, "cloned handle %d to new handle %d", n, out);
		}
	}
	/* Run all process initialization callbacks */
	LIST_FOREACH(&process_callbacks_init, pc, struct PROCESS_CALLBACK) {
		err = pc->pc_func(p);
		if (ananas_is_failure(err))
			goto fail;
	}

	/* Grab the parent's lock and insert the child */
	if (parent != NULL) {
		process_lock(parent);
		LIST_APPEND_IP(&parent->p_children, children, p);
		process_unlock(parent);
	}

	/* Finally, add the process to all processes */
	mutex_lock(&Ananas::Process::process_mtx);
	LIST_APPEND_IP(&Ananas::Process::process_all, all, p);
	mutex_unlock(&Ananas::Process::process_mtx);

	*dest = p;
	return ananas_success();

fail:
	vmspace_destroy(*p->p_vmspace);
	kfree(p);
	return err;
}

errorcode_t
process_alloc(process_t* parent, process_t** dest)
{
	return process_alloc_ex(parent, dest, 0);
}

errorcode_t
process_clone(process_t* p, int flags, process_t** out_p)
{
	errorcode_t err;
	process_t* newp;
	err = process_alloc_ex(p, &newp, 0);
	ANANAS_ERROR_RETURN(err);

	/* Duplicate the vmspace - this should leave the private mappings alone */
	err = vmspace_clone(*p->p_vmspace, *newp->p_vmspace, 0);
	if (ananas_is_failure(err))
		goto fail;

	*out_p = newp;
	return ananas_success();

fail:
	process_deref(newp);
	return err;
}

static void
process_destroy(process_t* p)
{
	/* Run all process exit callbacks */
	LIST_FOREACH(&process_callbacks_exit, pc, struct PROCESS_CALLBACK) {
		pc->pc_func(p);
	}

	/* Free all handles */
	for(unsigned int n = 0; n < PROCESS_MAX_HANDLES; n++)
		handle_free_byindex(p, n);

	/* Clean the process's vmspace up - this will remove all non-essential mappings */
	vmspace_cleanup(*p->p_vmspace);

	/* Remove the process from the all-process list */
	mutex_lock(&Ananas::Process::process_mtx);
	LIST_REMOVE_IP(&Ananas::Process::process_all, all, p);
	mutex_unlock(&Ananas::Process::process_mtx);

	/*
	 * Unmap the process information; no one can query it at this point as the
	 * process itself will not run anymore.
	 */
	kmem_unmap(p->p_info, sizeof(struct PROCINFO));
}

void
process_ref(process_t* p)
{
	KASSERT(p->p_refcount > 0, "reffing process with invalid refcount %d", p->p_refcount);
	++p->p_refcount;
}

void
process_deref(process_t* p)
{
	KASSERT(p->p_refcount > 0, "dereffing process with invalid refcount %d", p->p_refcount);

	if (--p->p_refcount == 0)
		process_destroy(p);
}

void
process_exit(process_t* p, int status)
{
	process_lock(p);
	p->p_state = PROCESS_STATE_ZOMBIE;
	p->p_exit_status = status;
	process_unlock(p);

	sem_signal(&Ananas::Process::process_sleep_sem);
}

errorcode_t
process_wait_and_lock(process_t* parent, int flags, process_t** p_out)
{
	if (flags != 0)
		return ANANAS_ERROR(BAD_FLAG);
	/*
	 * XXX We aren't going for efficiency here - thus we use a single
	 *     semaphore to wake anything up once a process has exited.
	 */
	for(;;) {
		process_lock(parent);
		LIST_FOREACH_IP(&parent->p_children, children, child, struct PROCESS) {
			process_lock(child);
			if (child->p_state == PROCESS_STATE_ZOMBIE) {
				/* Found one; remove it from the parent's list */
				LIST_REMOVE_IP(&parent->p_children, children, child);
				process_unlock(parent);

				/* Note that we give our ref to the caller! */
				*p_out = child;
				return ananas_success();
			}
			process_unlock(child);
		}
		process_unlock(parent);

		/* Nothing good yet; sleep on it */
		sem_wait(&Ananas::Process::process_sleep_sem);
	}

	/* NOTREACHED */
}

errorcode_t
process_set_args(process_t* p, const char* args, size_t args_len)
{
	if (args_len >= (PROCINFO_ARGS_LENGTH - 1))
		args_len = PROCINFO_ARGS_LENGTH - 1;
	for (unsigned int i = 0; i < args_len; i++)
		if(args[i] == '\0' && args[i + 1] == '\0') {
			memcpy(p->p_info->pi_args, args, i + 2 /* terminating \0\0 */);
			return ananas_success();
		}
	return ANANAS_ERROR(BAD_LENGTH);
}

errorcode_t
process_set_environment(process_t* p, const char* env, size_t env_len)
{
	if (env_len >= (PROCINFO_ENV_LENGTH - 1))
		env_len = PROCINFO_ENV_LENGTH - 1;
	for (unsigned int i = 0; i < env_len; i++)
		if(env[i] == '\0' && env[i + 1] == '\0') {
			memcpy(p->p_info->pi_env, env, i + 2 /* terminating \0\0 */);
			return ananas_success();
		}

	return ANANAS_ERROR(BAD_LENGTH);
}

process_t*
process_lookup_by_id_and_ref(pid_t pid)
{
	mutex_lock(&Ananas::Process::process_mtx);
	LIST_FOREACH_IP(&Ananas::Process::process_all, all, p, struct PROCESS) {
		process_lock(p);
		if (p->p_pid != pid) {
			process_unlock(p);
			continue;
		}

		// Process found; get a ref and return it
		process_ref(p);
		process_unlock(p);
		mutex_unlock(&Ananas::Process::process_mtx);
		return p;
	}
	mutex_unlock(&Ananas::Process::process_mtx);
	return nullptr;
}

errorcode_t
process_register_init_func(struct PROCESS_CALLBACK* fn)
{
	LIST_APPEND(&process_callbacks_init, fn);
	return ananas_success();
}

errorcode_t
process_register_exit_func(struct PROCESS_CALLBACK* fn)
{
	LIST_APPEND(&process_callbacks_exit, fn);
	return ananas_success();
}

errorcode_t
process_unregister_init_func(struct PROCESS_CALLBACK* fn)
{
	LIST_REMOVE(&process_callbacks_init, fn);
	return ananas_success();
}

errorcode_t
process_unregister_exit_func(struct PROCESS_CALLBACK* fn)
{
	LIST_REMOVE(&process_callbacks_exit, fn);
	return ananas_success();
}

static errorcode_t
process_init()
{
	mutex_init(&Ananas::Process::process_mtx, "proc");
	sem_init(&Ananas::Process::process_sleep_sem, 0);
	LIST_INIT(&Ananas::Process::process_all);
	Ananas::Process::process_curpid = 1;

	return ananas_success();
}

INIT_FUNCTION(process_init, SUBSYSTEM_PROCESS, ORDER_FIRST);

#ifdef OPTION_KDB

void vmspace_dump(VMSpace&);

KDB_COMMAND(ps, "[s:flags]", "Displays all processes")
{
	mutex_lock(&Ananas::Process::process_mtx);
	LIST_FOREACH_IP(&Ananas::Process::process_all, all, p, struct PROCESS) {
		kprintf("process %d (%p): state %d\n", p->p_pid, p, p->p_state);
		vmspace_dump(*p->p_vmspace);
	}
	mutex_unlock(&Ananas::Process::process_mtx);
}

#endif // OPTION_KDB

/* vim:set ts=2 sw=2: */
