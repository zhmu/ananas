#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/handle.h>
#include <ananas/kdb.h>
#include <ananas/lib.h>
#include <ananas/lock.h>
#include <ananas/mm.h>
#include <ananas/process.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/thread.h>
#include "options.h"

TRACE_SETUP;

#define NUM_HANDLES 500 /* XXX shouldn't be static */

static struct HANDLE_QUEUE handle_freelist;
static spinlock_t spl_handlequeue;
static struct HANDLE_TYPES handle_types;
static spinlock_t spl_handletypes;

void
handle_init()
{
	struct HANDLE* pool = kmalloc(sizeof(struct HANDLE) * (NUM_HANDLES + 1));
	spinlock_init(&spl_handlequeue);
	spinlock_init(&spl_handletypes);
	LIST_INIT(&handle_types);

	/*
	 * Ensure handle pool is aligned; this makes checking for valid handles
	 * easier.
	 */
	pool = (struct HANDLE*)((addr_t)pool + (sizeof(struct HANDLE) - (addr_t)pool % sizeof(struct HANDLE)));
	memset(pool, 0, sizeof(struct HANDLE) * NUM_HANDLES);

	/* Add all handles to the queue one by one */
	LIST_INIT(&handle_freelist);
	for (unsigned int i = 0; i < NUM_HANDLES; i++) {
		struct HANDLE* h = &pool[i];
		LIST_APPEND(&handle_freelist, h);
	}
}

errorcode_t
handle_alloc(int type, process_t* proc, handleindex_t index_from, struct HANDLE** handle_out, handleindex_t* index_out)
{
	KASSERT(proc != NULL, "handle_alloc() without process");

	/* Look up the handle type XXX O(n) */
	struct HANDLE_TYPE* htype = NULL;
	spinlock_lock(&spl_handletypes);
	LIST_FOREACH(&handle_types, ht, struct HANDLE_TYPE) {
		if (ht->ht_id != type)
			continue;
		htype = ht;
		break;
	}
	spinlock_unlock(&spl_handletypes);
	if (htype == NULL)
		return ANANAS_ERROR(BAD_TYPE);

	/* Grab a handle from the pool */
	spinlock_lock(&spl_handlequeue);
	if (LIST_EMPTY(&handle_freelist)) {
		/* XXX should we wait for a new handle, or just give an error? */
		panic("out of handles");
	}
	struct HANDLE* handle = LIST_HEAD(&handle_freelist);
	LIST_POP_HEAD(&handle_freelist);
	spinlock_unlock(&spl_handlequeue);

	/* Sanity checks */
	KASSERT(handle->h_type == HANDLE_TYPE_UNUSED, "handle from pool must be unused");

	/* Initialize the handle */
	mutex_init(&handle->h_mutex, "handle");
	handle->h_type = type;
	handle->h_process = proc;
	handle->h_hops = htype->ht_hops;
	handle->h_flags = 0;

	/* Hook the handle to the process */
	process_lock(proc);
	handleindex_t n = index_from;
	while(n < PROCESS_MAX_HANDLES && proc->p_handle[n] != NULL)
		n++;
	if (n < PROCESS_MAX_HANDLES)
		proc->p_handle[n] = handle;
	process_unlock(proc);

	if (n == PROCESS_MAX_HANDLES)
		panic("out of handles"); // XXX FIX ME

	*handle_out = handle;
	*index_out = n;
	TRACE(HANDLE, INFO, "process=%p, type=%u => handle=%p, index=%u", proc, type, handle, n);
	return ANANAS_ERROR_NONE;
}

errorcode_t
handle_lookup(process_t* proc, handleindex_t index, int type, struct HANDLE** handle_out)
{
	KASSERT(proc != NULL, "handle_lookup() without process");
	if(index < 0 || index >= PROCESS_MAX_HANDLES)
		return ANANAS_ERROR(BAD_HANDLE);

	/* Obtain the handle XXX How do we ensure it won't get freed after this? Should we ref it? */
	process_lock(proc);
	struct HANDLE* handle = proc->p_handle[index];
	process_unlock(proc);

	/* ensure handle exists - we don't verify ownership: it _is_ in the thread's handle table... */
	if (handle == NULL)
		return ANANAS_ERROR(BAD_HANDLE);
	/* if this is a handle reference, check the type of the handle we are refering to */
	if (type != HANDLE_TYPE_ANY && handle->h_type != type)
		return ANANAS_ERROR(BAD_HANDLE);
	/* ... yet unused handles are never valid */
	if (handle->h_type == HANDLE_TYPE_UNUSED)
		return ANANAS_ERROR(BAD_HANDLE);
	*handle_out = handle;
	return ANANAS_ERROR_NONE;
}

errorcode_t
handle_free(struct HANDLE* handle)
{
	/*
	 * Lock the handle so that no-one else can touch it, mark the handle as being
	 * torn-down and see if we actually have to destroy it at this point.
	 */
	mutex_lock(&handle->h_mutex);

	/* Remove us from the thread handle queue, if necessary */
	process_t* proc = handle->h_process;
	if (proc != NULL) {
		process_lock(proc);
		for (handleindex_t n = 0; n < PROCESS_MAX_HANDLES; n++) {
			if (proc->p_handle[n] == handle)
				proc->p_handle[n] = NULL;
		}
		process_unlock(proc);
	}

	/*
	 * If the handle has a specific free function, call it - otherwise assume
	 * no special action is needed.
	 */
	if (handle->h_hops->hop_free != NULL) {
		errorcode_t err = handle->h_hops->hop_free(proc, handle);
		ANANAS_ERROR_RETURN(err);
	}

	/* Clear the handle */
	memset(&handle->h_data, 0, sizeof(handle->h_data));
	handle->h_type = HANDLE_TYPE_UNUSED; /* just to ensure the value matches */
	handle->h_process = NULL;

	/*	
	 * Let go of the handle lock - if someone tries to use it, they'll lock it
	 * before looking at the flags field and this will cause a deadlock.  Better
	 * safe than sorry.
	 */
	mutex_unlock(&handle->h_mutex);

	/* Hand it back to the the pool */
	spinlock_lock(&spl_handlequeue);
	LIST_APPEND(&handle_freelist, handle);
	spinlock_unlock(&spl_handlequeue);
	return ANANAS_ERROR_NONE;
}

errorcode_t
handle_free_byindex(process_t* proc, handleindex_t index)
{
	/* Look the handle up */
	struct HANDLE* handle;
	errorcode_t err = handle_lookup(proc, index, HANDLE_TYPE_ANY, &handle);
	ANANAS_ERROR_RETURN(err);

	return handle_free(handle);
}

errorcode_t
handle_clone_generic(struct HANDLE* handle_in, process_t* proc_out, struct HANDLE** handle_out, handleindex_t index_out_min, handleindex_t* index_out)
{
	errorcode_t err = handle_alloc(handle_in->h_type, proc_out, index_out_min, handle_out, index_out);
	ANANAS_ERROR_RETURN(err);
	memcpy(&(*handle_out)->h_data, &handle_in->h_data, sizeof(handle_in->h_data));
	return ANANAS_ERROR_NONE;
}

errorcode_t
handle_clone(process_t* proc_in, handleindex_t index, struct CLONE_OPTIONS* opts, process_t* proc_out, struct HANDLE** handle_out, handleindex_t index_out_min, handleindex_t* index_out)
{
	struct HANDLE* handle;
	errorcode_t err = handle_lookup(proc_in, index, HANDLE_TYPE_ANY, &handle);
	ANANAS_ERROR_RETURN(err);

	mutex_lock(&handle->h_mutex);
	if (handle->h_hops->hop_clone != NULL) {
		err = handle->h_hops->hop_clone(proc_in, index, handle, opts, proc_out, handle_out, index_out_min, index_out);
	} else {
		err = ANANAS_ERROR(BAD_OPERATION);
	}
	mutex_unlock(&handle->h_mutex);
	return err;
}

errorcode_t
handle_register_type(struct HANDLE_TYPE* ht)
{
	spinlock_lock(&spl_handletypes);
	LIST_APPEND(&handle_types, ht);
	spinlock_unlock(&spl_handletypes);
	return ANANAS_ERROR_NONE;
}

errorcode_t
handle_unregister_type(struct HANDLE_TYPE* ht)
{
	spinlock_lock(&spl_handletypes);
	LIST_REMOVE(&handle_types, ht);
	spinlock_unlock(&spl_handletypes);
	return ANANAS_ERROR_NONE;
}

#ifdef OPTION_KDB
KDB_COMMAND(handle, "i:handle", "Display handle information")
{
	struct HANDLE* handle = (void*)arg[1].a_u.u_value;
	kprintf("type          : %u\n", handle->h_type);
	kprintf("flags         : %u\n", handle->h_flags);
	kprintf("owner process : 0x%p\n", handle->h_process);
	switch(handle->h_type) {
		case HANDLE_TYPE_FILE: {
			kprintf("file handle specifics:\n");
			kprintf("   offset         : %u\n", handle->h_data.d_vfs_file.f_offset); /* XXXSIZE */
			kprintf("   dentry         : 0x%x\n", handle->h_data.d_vfs_file.f_dentry);
			kprintf("   device         : 0x%x\n", handle->h_data.d_vfs_file.f_device);
			break;
		}
	}
}
#endif

/* vim:set ts=2 sw=2: */
