#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/page.h>
#include <ananas/handle.h>
#include <ananas/process.h>
#include <ananas/procinfo.h>
#include <ananas/vm.h>
#include <ananas/vmspace.h>
#include <machine/param.h> /* for PAGE_SIZE */

TRACE_SETUP;

/* XXX These should be locked */
static struct PROCESS_CALLBACKS process_callbacks_init;
static struct PROCESS_CALLBACKS process_callbacks_exit;

static semaphore_t process_sleep_sem;
static mutex_t process_mtx;
static struct PROCESS_QUEUE process_all;
static pid_t process_curpid = -1;

static pid_t
process_alloc_pid()
{
	/* XXX this is a bit of a kludge for now ... */
	mutex_lock(&process_mtx);
	pid_t pid = process_curpid++;
	mutex_unlock(&process_mtx);

	return pid;
}

static errorcode_t
process_alloc_ex(process_t* parent, process_t** dest, int flags)
{
	errorcode_t err;

	process_t* p = kmalloc(sizeof(struct PROCESS));
	memset(p, 0, sizeof(*p));
	p->p_parent = parent; /* XXX should we take a ref here? */
	p->p_refcount = 1; /* caller */
	p->p_state = PROCESS_STATE_ACTIVE;
	p->p_pid = process_alloc_pid();
	mutex_init(&p->p_lock, "plock");
	LIST_INIT(&p->p_children);

	/* Create the process's vmspace */
	err = vmspace_create(&p->p_vmspace);
	if (err != ANANAS_ERROR_NONE)
		goto fail;

	/* Create process information structure */
	p->p_info = page_alloc_length_mapped(sizeof(struct PROCINFO), &p->p_info_page, VM_FLAG_READ | VM_FLAG_WRITE);
	if (p->p_info == NULL) {
		err = ANANAS_ERROR(OUT_OF_MEMORY);
		goto fail;
	}

	/* Map the process information structure in the vm area so all out threads can access it */
	vmarea_t* va;
	err = vmspace_map(p->p_vmspace, page_get_paddr(p->p_info_page), sizeof(struct PROCINFO), VM_FLAG_USER | VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_PRIVATE, &va);
	if (err != ANANAS_ERROR_NONE)
		goto fail;
	p->p_info_va = va->va_virt;

	/* Initialize thread information structure */
	memset(p->p_info, 0, sizeof(struct PROCINFO));
	p->p_info->pi_size = sizeof(struct PROCINFO);
	p->p_info->pi_pid = p->p_pid;
	if (parent != NULL)
		process_set_environment(p, parent->p_info->pi_env, PROCINFO_ENV_LENGTH - 1);

	/* Clone the parent's handles - we skip the thread handle */
	if (parent != NULL) {
		for (unsigned int n = 0; n < PROCESS_MAX_HANDLES; n++) {
			if (parent->p_handle[n] == NULL)
				continue;

			struct HANDLE* handle;
			handleindex_t out;
			err = handle_clone(parent, n, NULL, p, &handle, n, &out);
			if (err != ANANAS_ERROR_NONE)
				goto fail;
			KASSERT(n == out, "cloned handle %d to new handle %d", n, out);
		}
	}
	/* Run all process initialization callbacks */
	LIST_FOREACH(&process_callbacks_init, pc, struct PROCESS_CALLBACK) {
		err = pc->pc_func(p);
		if (err != ANANAS_ERROR_NONE)
			goto fail;
	}

	/* Grab the parent's lock and insert the child */
	if (parent != NULL) {
		process_lock(parent);
		LIST_APPEND_IP(&parent->p_children, children, p);
		process_unlock(parent);
	}

	/* Finally, add the process to all processes */
	mutex_lock(&process_mtx);
	LIST_APPEND_IP(&process_all, all, p);
	mutex_unlock(&process_mtx);

	*dest = p;
	return ananas_success();

fail:
	if (p->p_info != NULL)
		page_free(p->p_info_page);
	if (p->p_vmspace != NULL)
		vmspace_destroy(p->p_vmspace);
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
	err = vmspace_clone(p->p_vmspace, newp->p_vmspace, 0);
	if (err != ANANAS_ERROR_NONE)
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

	/* Clean the thread's vmspace up - this will remove all non-essential mappings */
	vmspace_cleanup(p->p_vmspace);

	/* Remove the process from the all-process list */
	mutex_lock(&process_mtx);
	LIST_REMOVE_IP(&process_all, all, p);
	mutex_unlock(&process_mtx);

	/*
	 * Clear the process information; no one can query it at this point as the
	 * thread itself will not run anymore.
	 */
	page_free(p->p_info_page);
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

	sem_signal(&process_sleep_sem);
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
		sem_wait(&process_sleep_sem);
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
	mutex_init(&process_mtx, "proc");
	sem_init(&process_sleep_sem, 0);
	LIST_INIT(&process_all);
	process_curpid = 1;

	return ananas_success();
}

INIT_FUNCTION(process_init, SUBSYSTEM_PROCESS, ORDER_FIRST);

/* vim:set ts=2 sw=2: */
