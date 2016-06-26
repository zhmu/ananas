#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/handle.h>
#include <ananas/kdb.h>
#include <ananas/lib.h>
#include <ananas/lock.h>
#include <ananas/mm.h>
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
	DQUEUE_INIT(&handle_types);

	/*
	 * Ensure handle pool is aligned; this makes checking for valid handles
	 * easier.
	 */
	pool = (struct HANDLE*)((addr_t)pool + (sizeof(struct HANDLE) - (addr_t)pool % sizeof(struct HANDLE)));
	memset(pool, 0, sizeof(struct HANDLE) * NUM_HANDLES);

	/* Add all handles to the queue one by one */
	DQUEUE_INIT(&handle_freelist);
	for (unsigned int i = 0; i < NUM_HANDLES; i++) {
		struct HANDLE* h = &pool[i];
		DQUEUE_ADD_TAIL(&handle_freelist, h);
	}

	/*
	 * XXX Thread handles are often used during the boot process to make a handle
	 * for the idle process; this means we cannot register them using the sysinit
	 * framework - thus kludge and register them here.
	 */
	extern struct HANDLE_TYPE thread_handle_type;
	handle_register_type(&thread_handle_type);
}

errorcode_t
handle_alloc(int type, thread_t* t, handleindex_t index_from, struct HANDLE** handle_out, handleindex_t* index_out)
{
	KASSERT(t != NULL, "handle_alloc() without thread");

	/* Look up the handle type XXX O(n) */
	struct HANDLE_TYPE* htype = NULL;
	spinlock_lock(&spl_handletypes);
	if (!DQUEUE_EMPTY(&handle_types)) {
		DQUEUE_FOREACH(&handle_types, ht, struct HANDLE_TYPE) {
			if (ht->ht_id != type)
				continue;
			htype = ht;
			break;
		}
	}
	spinlock_unlock(&spl_handletypes);
	if (htype == NULL)
		return ANANAS_ERROR(BAD_TYPE);

	/* Grab a handle from the pool */
	spinlock_lock(&spl_handlequeue);
	if (DQUEUE_EMPTY(&handle_freelist)) {
		/* XXX should we wait for a new handle, or just give an error? */
		panic("out of handles");
	}
	struct HANDLE* handle = DQUEUE_HEAD(&handle_freelist);
	DQUEUE_POP_HEAD(&handle_freelist);
	spinlock_unlock(&spl_handlequeue);

	/* Sanity checks */
	KASSERT(handle->h_type == HANDLE_TYPE_UNUSED, "handle from pool must be unused");

	/* Initialize the handle */
	mutex_init(&handle->h_mutex, "handle");
	handle->h_type = type;
	handle->h_thread = t;
	handle->h_hops = htype->ht_hops;
	handle->h_flags = 0;

	/* Hook the handle to the thread */
	spinlock_lock(&t->t_lock);
	handleindex_t n = index_from;
	while(n < THREAD_MAX_HANDLES && t->t_handle[n] != NULL)
		n++;
	if (n < THREAD_MAX_HANDLES)
		t->t_handle[n] = handle;
	spinlock_unlock(&t->t_lock);

	if (n == THREAD_MAX_HANDLES)
		panic("out of handles"); // XXX FIX ME

	KASSERT(((addr_t)handle % sizeof(struct HANDLE)) == 0, "handle address %p is not 0x%x byte aligned", (addr_t)handle, sizeof(struct HANDLE));
	*handle_out = handle;
	*index_out = n;
	TRACE(HANDLE, INFO, "thread=%p, type=%u => handle=%p, index=%u", t, type, handle, n);
	return ANANAS_ERROR_OK;
}

errorcode_t
handle_lookup(thread_t* t, handleindex_t index, int type, struct HANDLE** handle_out)
{
	if(index < 0 || index >= THREAD_MAX_HANDLES)
		return ANANAS_ERROR(BAD_HANDLE);

	/* Obtain the handle XXX How do we ensure it won't get freed after this? Should we ref it? */
	spinlock_lock(&t->t_lock);
	struct HANDLE* handle = t->t_handle[index];
	spinlock_unlock(&t->t_lock);

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
	return ANANAS_ERROR_OK;
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
	if (handle->h_thread != NULL) {
		spinlock_lock(&handle->h_thread->t_lock);
		for (handleindex_t n = 0; n < THREAD_MAX_HANDLES; n++) {
			if (handle->h_thread->t_handle[n] == handle)
				handle->h_thread->t_handle[n] = NULL;
		}
		spinlock_unlock(&handle->h_thread->t_lock);
	}

	/*
	 * If the handle has a specific free function, call it - otherwise assume
	 * no special action is needed.
	 */
	if (handle->h_hops->hop_free != NULL) {
		errorcode_t err = handle->h_hops->hop_free(handle->h_thread, handle);
		ANANAS_ERROR_RETURN(err);
	}

	/* Clear the handle */
	memset(&handle->h_data, 0, sizeof(handle->h_data));
	handle->h_type = HANDLE_TYPE_UNUSED; /* just to ensure the value matches */
	handle->h_thread = NULL;

	/*	
	 * Let go of the handle lock - if someone tries to use it, they'll lock it
	 * before looking at the flags field and this will cause a deadlock.  Better
	 * safe than sorry.
	 */
	mutex_unlock(&handle->h_mutex);

	/* Hand it back to the the pool */
	spinlock_lock(&spl_handlequeue);
	DQUEUE_ADD_TAIL(&handle_freelist, handle);
	spinlock_unlock(&spl_handlequeue);
	return ANANAS_ERROR_OK;
}

errorcode_t
handle_free_byindex(thread_t* t, handleindex_t index)
{
	/* Look the handle up */
	struct HANDLE* handle;
	errorcode_t err = handle_lookup(t, index, HANDLE_TYPE_ANY, &handle);
	ANANAS_ERROR_RETURN(err);

	return handle_free(handle);
}

errorcode_t
handle_clone_generic(struct HANDLE* handle_in, thread_t* thread_out, struct HANDLE** handle_out, handleindex_t index_out_min, handleindex_t* index_out)
{
	errorcode_t err = handle_alloc(handle_in->h_type, thread_out, index_out_min, handle_out, index_out);
	ANANAS_ERROR_RETURN(err);
	memcpy(&(*handle_out)->h_data, &handle_in->h_data, sizeof(handle_in->h_data));
	return ANANAS_ERROR_OK;
}

errorcode_t
handle_clone(thread_t* thread_in, handleindex_t index, struct CLONE_OPTIONS* opts, thread_t* thread_out, struct HANDLE** handle_out, handleindex_t index_out_min, handleindex_t* index_out)
{
	struct HANDLE* handle;
	errorcode_t err = handle_lookup(thread_in, index, HANDLE_TYPE_ANY, &handle);
	ANANAS_ERROR_RETURN(err);

	mutex_lock(&handle->h_mutex);
	if (handle->h_hops->hop_clone != NULL) {
		err = handle->h_hops->hop_clone(thread_in, index, handle, opts, thread_out, handle_out, index_out_min, index_out);
	} else {
		err = ANANAS_ERROR(BAD_OPERATION);
	}
	mutex_unlock(&handle->h_mutex);
	return err;
}

errorcode_t
handle_set_owner(thread_t* t, handleindex_t index_in, handleindex_t owner_thread_in, handleindex_t* index_out)
{
	errorcode_t err;

	/* Fetch the owner thread */
	struct HANDLE* owner_handle;
	err = handle_lookup(t, owner_thread_in, HANDLE_TYPE_THREAD, &owner_handle);
	ANANAS_ERROR_RETURN(err);
	thread_t* new_thread = owner_handle->h_data.d_thread;

	/* XXX We should check the relationship between the current and new thread */
	
	/* Fetch the handle */
	struct HANDLE* handle_in;
	err = handle_lookup(t, index_in, HANDLE_TYPE_ANY, &handle_in);
	ANANAS_ERROR_RETURN(err);

	/* Remove the thread from the old thread's handles */
	thread_t* old_thread = handle_in->h_thread;
	spinlock_lock(&old_thread->t_lock);
	old_thread->t_handle[index_in] = NULL;
	spinlock_unlock(&old_thread->t_lock);

	/* And hook it up the new thread's handles */
	spinlock_lock(&new_thread->t_lock);
	handleindex_t n = 0;
	for(/* nothing */; n < THREAD_MAX_HANDLES; n++)
		if (new_thread->t_handle[n] == NULL)
			break;
	KASSERT(n < THREAD_MAX_HANDLES, "out of handles"); /* XXX deal with this */
	new_thread->t_handle[n] = handle_in;
	handle_in->h_thread = new_thread;
	spinlock_unlock(&new_thread->t_lock);

	*index_out = n;
	return ANANAS_ERROR_OK;
}

errorcode_t
handle_register_type(struct HANDLE_TYPE* ht)
{
	spinlock_lock(&spl_handletypes);
	DQUEUE_ADD_TAIL(&handle_types, ht);
	spinlock_unlock(&spl_handletypes);
	return ANANAS_ERROR_OK;
}

errorcode_t
handle_unregister_type(struct HANDLE_TYPE* ht)
{
	spinlock_lock(&spl_handletypes);
	DQUEUE_REMOVE(&handle_types, ht);
	spinlock_unlock(&spl_handletypes);
	return ANANAS_ERROR_OK;
}

#ifdef OPTION_KDB
KDB_COMMAND(handle, "i:handle", "Display handle information")
{
	struct HANDLE* handle = (void*)arg[1].a_u.u_value;
	kprintf("type          : %u\n", handle->h_type);
	kprintf("flags         : %u\n", handle->h_flags);
	kprintf("owner thread  : 0x%x\n", handle->h_thread);
	switch(handle->h_type) {
		case HANDLE_TYPE_FILE: {
			kprintf("file handle specifics:\n");
			kprintf("   offset         : %u\n", handle->h_data.d_vfs_file.f_offset); /* XXXSIZE */
			kprintf("   dentry         : 0x%x\n", handle->h_data.d_vfs_file.f_dentry);
			kprintf("   device         : 0x%x\n", handle->h_data.d_vfs_file.f_device);
			break;
		}
		case HANDLE_TYPE_THREAD: {
			kprintf("thread handle specifics:\n");
			kprintf("   thread         : 0x%x\n", handle->h_data.d_thread);
			break;
		}
	}
}
#endif

/* vim:set ts=2 sw=2: */
