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

errorcode_t
process_alloc(process_t* parent, process_t** dest)
{
	errorcode_t err;

	process_t* p = kmalloc(sizeof(struct PROCESS));
	memset(p, 0, sizeof(*p));
	p->p_refcount = 1; /* caller */
	mutex_init(&p->p_lock, "plock");

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
	if (parent != NULL)
		process_set_environment(p, parent->p_info->pi_env, PAGE_SIZE /* XXX */);
	p->p_info->pi_handle_main = -1; /* no main thread handle yet */

	/* Clone the parent's handles - we skip the thread handle */
	if (parent != NULL) {
		for (unsigned int n = 0; n < PROCESS_MAX_HANDLES; n++) {
			if (n == parent->p_info->pi_handle_main)
				continue; /* do not clone the main thread handle */
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
	if (!DQUEUE_EMPTY(&process_callbacks_init))
		DQUEUE_FOREACH(&process_callbacks_init, pc, struct PROCESS_CALLBACK) {
			err = pc->pc_func(p);
			if (err != ANANAS_ERROR_NONE)
				goto fail;
		}

	*dest = p;
	return ANANAS_ERROR_OK;

fail:
	if (p->p_info != NULL)
		page_free(p->p_info_page);
	if (p->p_vmspace != NULL)
		vmspace_destroy(p->p_vmspace);
	kfree(p);
	return err;
}

errorcode_t
process_clone(process_t* p, process_t** out_p)
{

	process_t* newp = kmalloc(sizeof(struct PROCESS));
	memset(newp, 0, sizeof(*newp));
	newp->p_refcount = 1; /* caller */

	errorcode_t err = vmspace_create(&p->p_vmspace);
	if (err != ANANAS_ERROR_NONE)
		goto fail;

	/* Duplicate the vmspace */
	err = vmspace_clone(p->p_vmspace, newp->p_vmspace);
	if (err != ANANAS_ERROR_OK)
		goto fail;

fail:
	kfree(newp);
	return err;
}

static void
process_destroy(process_t* p)
{
	/* Run all process exit callbacks */
	if (!DQUEUE_EMPTY(&process_callbacks_exit))
		DQUEUE_FOREACH(&process_callbacks_exit, pc, struct PROCESS_CALLBACK) {
			pc->pc_func(p);
		}

	/* Free all handles */
	for(unsigned int n = 0; n < PROCESS_MAX_HANDLES; n++)
		handle_free_byindex(p, n);

	/* Clean the thread's vmspace up - this will remove all non-essential mappings */
	vmspace_cleanup(p->p_vmspace);

	/*
	 * Clear the process information; no one can query it at this point as the
	 * thread itself will not run anymore.
	 */
	page_free(p->p_info_page);
}

void
process_ref(process_t* p)
{
	kprintf("process_ref(): p=%p\n", p);
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

errorcode_t
process_set_args(process_t* p, const char* args, size_t args_len)
{
	if (args_len >= (PROCINFO_ARGS_LENGTH - 1))
		args_len = PROCINFO_ARGS_LENGTH - 1;
	for (unsigned int i = 0; i < args_len; i++)
		if(args[i] == '\0' && args[i + 1] == '\0') {
			memcpy(p->p_info->pi_args, args, i + 2 /* terminating \0\0 */);
			return ANANAS_ERROR_OK;
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
			return ANANAS_ERROR_OK;
		}

	return ANANAS_ERROR(BAD_LENGTH);
}

errorcode_t
process_register_init_func(struct PROCESS_CALLBACK* fn)
{
	DQUEUE_ADD_TAIL(&process_callbacks_init, fn);
	return ANANAS_ERROR_OK;
}

errorcode_t
process_register_exit_func(struct PROCESS_CALLBACK* fn)
{
	DQUEUE_ADD_TAIL(&process_callbacks_exit, fn);
	return ANANAS_ERROR_OK;
}

errorcode_t
process_unregister_init_func(struct PROCESS_CALLBACK* fn)
{
	DQUEUE_REMOVE(&process_callbacks_init, fn);
	return ANANAS_ERROR_OK;
}

errorcode_t
process_unregister_exit_func(struct PROCESS_CALLBACK* fn)
{
	DQUEUE_REMOVE(&process_callbacks_exit, fn);
	return ANANAS_ERROR_OK;
}

/* vim:set ts=2 sw=2: */
