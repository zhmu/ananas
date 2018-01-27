#include <ananas/types.h>
#include <ananas/error.h>
#include "options.h"
#include "kernel/handle.h"
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/lock.h"
#include "kernel/process.h"
#include "kernel/trace.h"

TRACE_SETUP;

#define NUM_HANDLES 500 /* XXX shouldn't be static */

static HandleList handle_freelist;
static Spinlock spl_handlequeue;
static HandleTypeList handle_types;
static Spinlock spl_handletypes;

void
handle_init()
{
	spinlock_init(spl_handlequeue);
	spinlock_init(spl_handletypes);

	auto pool = new HANDLE[NUM_HANDLES];
	memset(pool, 0, sizeof(struct HANDLE) * NUM_HANDLES);

	/* Add all handles to the queue one by one */
	for (unsigned int i = 0; i < NUM_HANDLES; i++) {
		struct HANDLE* h = &pool[i];
		handle_freelist.push_back(*h);
	}
}

errorcode_t
handle_alloc(int type, Process& proc, handleindex_t index_from, struct HANDLE** handle_out, handleindex_t* index_out)
{
	/* Look up the handle type XXX O(n) */
	HandleType* htype = nullptr;
	spinlock_lock(spl_handletypes);
	for(auto& ht: handle_types) {
		if (ht.ht_id != type)
			continue;
		htype = &ht;
		break;
	}
	spinlock_unlock(spl_handletypes);
	if (htype == nullptr)
		return ANANAS_ERROR(BAD_TYPE);

	/* Grab a handle from the pool */
	spinlock_lock(spl_handlequeue);
	if (handle_freelist.empty()) {
		/* XXX should we wait for a new handle, or just give an error? */
		panic("out of handles");
	}
	struct HANDLE* handle = &handle_freelist.front();
	handle_freelist.pop_front();
	spinlock_unlock(spl_handlequeue);

	/* Sanity checks */
	KASSERT(handle->h_type == HANDLE_TYPE_UNUSED, "handle from pool must be unused");

	/* Initialize the handle */
	mutex_init(handle->h_mutex, "handle");
	handle->h_type = type;
	handle->h_process = &proc;
	handle->h_hops = htype->ht_hops;
	handle->h_flags = 0;

	/* Hook the handle to the process */
	process_lock(proc);
	handleindex_t n = index_from;
	while(n < PROCESS_MAX_HANDLES && proc.p_handle[n] != nullptr)
		n++;
	if (n < PROCESS_MAX_HANDLES)
		proc.p_handle[n] = handle;
	process_unlock(proc);

	if (n == PROCESS_MAX_HANDLES)
		panic("out of handles"); // XXX FIX ME

	*handle_out = handle;
	*index_out = n;
	TRACE(HANDLE, INFO, "process=%p, type=%u => handle=%p, index=%u", &proc, type, handle, n);
	return ananas_success();
}

errorcode_t
handle_lookup(Process& proc, handleindex_t index, int type, struct HANDLE** handle_out)
{
	if(index < 0 || index >= PROCESS_MAX_HANDLES)
		return ANANAS_ERROR(BAD_HANDLE);

	/* Obtain the handle XXX How do we ensure it won't get freed after this? Should we ref it? */
	process_lock(proc);
	struct HANDLE* handle = proc.p_handle[index];
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
	return ananas_success();
}

errorcode_t
handle_free(struct HANDLE* handle)
{
	/*
	 * Lock the handle so that no-one else can touch it, mark the handle as being
	 * torn-down and see if we actually have to destroy it at this point.
	 */
	mutex_lock(handle->h_mutex);

	/* Remove us from the thread handle queue, if necessary */
	Process* proc = handle->h_process;
	if (proc != nullptr) {
		process_lock(*proc);
		for (handleindex_t n = 0; n < PROCESS_MAX_HANDLES; n++) {
			if (proc->p_handle[n] == handle)
				proc->p_handle[n] = NULL;
		}
		process_unlock(*proc);
	}

	/*
	 * If the handle has a specific free function, call it - otherwise assume
	 * no special action is needed.
	 */
	if (handle->h_hops->hop_free != NULL) {
		errorcode_t err = handle->h_hops->hop_free(*proc, handle);
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
	mutex_unlock(handle->h_mutex);

	/* Hand it back to the the pool */
	spinlock_lock(spl_handlequeue);
	handle_freelist.push_back(*handle);
	spinlock_unlock(spl_handlequeue);
	return ananas_success();
}

errorcode_t
handle_free_byindex(Process& proc, handleindex_t index)
{
	/* Look the handle up */
	struct HANDLE* handle;
	errorcode_t err = handle_lookup(proc, index, HANDLE_TYPE_ANY, &handle);
	ANANAS_ERROR_RETURN(err);

	return handle_free(handle);
}

errorcode_t
handle_clone_generic(struct HANDLE* handle_in, Process& proc_out, struct HANDLE** handle_out, handleindex_t index_out_min, handleindex_t* index_out)
{
	errorcode_t err = handle_alloc(handle_in->h_type, proc_out, index_out_min, handle_out, index_out);
	ANANAS_ERROR_RETURN(err);
	memcpy(&(*handle_out)->h_data, &handle_in->h_data, sizeof(handle_in->h_data));
	return ananas_success();
}

errorcode_t
handle_clone(Process& proc_in, handleindex_t index, struct CLONE_OPTIONS* opts, Process& proc_out, struct HANDLE** handle_out, handleindex_t index_out_min, handleindex_t* index_out)
{
	struct HANDLE* handle;
	errorcode_t err = handle_lookup(proc_in, index, HANDLE_TYPE_ANY, &handle);
	ANANAS_ERROR_RETURN(err);

	mutex_lock(handle->h_mutex);
	if (handle->h_hops->hop_clone != NULL) {
		err = handle->h_hops->hop_clone(proc_in, index, handle, opts, proc_out, handle_out, index_out_min, index_out);
	} else {
		err = ANANAS_ERROR(BAD_OPERATION);
	}
	mutex_unlock(handle->h_mutex);
	return err;
}

errorcode_t
handle_register_type(HandleType& ht)
{
	spinlock_lock(spl_handletypes);
	handle_types.push_back(ht);
	spinlock_unlock(spl_handletypes);
	return ananas_success();
}

errorcode_t
handle_unregister_type(HandleType& ht)
{
	spinlock_lock(spl_handletypes);
	handle_types.remove(ht);
	spinlock_unlock(spl_handletypes);
	return ananas_success();
}

#ifdef OPTION_KDB
KDB_COMMAND(handle, "i:handle", "Display handle information")
{
	struct HANDLE* handle = static_cast<struct HANDLE*>((void*)(uintptr_t)arg[1].a_u.u_value);
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
