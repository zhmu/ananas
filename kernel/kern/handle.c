#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/handle.h>
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
static void *handle_memory_start = NULL;
static void *handle_memory_end = NULL;

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
		for (unsigned int n = 0; n < HANDLE_MAX_WAITERS; n++)
			sem_init(&h->h_waiters[n].hw_sem, 0);
	}

	/* Store the handle pool range; this allows us to quickly verify whether a handle is valid */
	handle_memory_start = pool;
	handle_memory_end   = (char*)pool + sizeof(struct HANDLE) * NUM_HANDLES;

	/*
	 * XXX Thread handles are often used during the boot process to make a handle
	 * for the idle process; this means we cannot register them using the sysinit
	 * framework - thus kludge and register them here.
	 */
	extern struct HANDLE_TYPE thread_handle_type;
	handle_register_type(&thread_handle_type);
}

errorcode_t
handle_alloc(int type, thread_t* t, struct HANDLE** out)
{
	KASSERT(handle_memory_start != NULL, "handle_alloc() without pool");
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
	handle->h_refcount = 1;

	/* Hook the handle to the thread queue */
	spinlock_lock(&t->t_lock);
	DQUEUE_ADD_TAIL(&t->t_handles, handle);
	spinlock_unlock(&t->t_lock);

	KASSERT(((addr_t)handle % sizeof(struct HANDLE)) == 0, "handle address %p is not 0x%x byte aligned", (addr_t)handle, sizeof(struct HANDLE));
	*out = handle;
	TRACE(HANDLE, INFO, "thread=%p, type=%u => handle=%p", t, type, handle);
	return ANANAS_ERROR_OK;
}

errorcode_t
handle_create_ref_locked(thread_t* t, struct HANDLE* h, struct HANDLE** out)
{
	/*
	 * First of all, increment the reference count of the source handle
	 * so that we can be certain that it won't go away
	 */
	KASSERT(h->h_refcount > 0, "source handle has invalid refcount");
	if (h->h_flags & HANDLE_FLAG_TEARDOWN) {
		/* Handle is in the process of being removed; disallow to create a reference */
		return ANANAS_ERROR(BAD_HANDLE);
	}
	h->h_refcount++;

	/* Now hook us up */
	errorcode_t err = handle_alloc(HANDLE_TYPE_REFERENCE, t, out);
	ANANAS_ERROR_RETURN(err);
	(*out)->h_data.d_handle = h;
	return ANANAS_ERROR_OK;
}

errorcode_t
handle_create_ref(thread_t* t, struct HANDLE* h, struct HANDLE** out)
{
	mutex_lock(&h->h_mutex);
	errorcode_t err = handle_create_ref_locked(t, h, out);
	mutex_unlock(&h->h_mutex);
	return err;
}

errorcode_t
handle_free(struct HANDLE* handle)
{
	KASSERT(handle->h_type != HANDLE_TYPE_UNUSED, "freeing free handle");
	TRACE(HANDLE, FUNC, "handle=%p", handle);

	/*
	 * Lock the handle so that no-one else can touch it, mark the handle as being
	 * torn-down and see if we actually have to destroy it at this point.
	 */
	mutex_lock(&handle->h_mutex);
	handle->h_flags |= HANDLE_FLAG_TEARDOWN;
	handle->h_refcount--;
	KASSERT(handle->h_refcount >= 0, "handle has invalid refcount");
	if (handle->h_refcount > 0) {
		TRACE(HANDLE, INFO, "handle=%p, not freeing (refcount %d)", handle, handle->h_refcount);
		mutex_unlock(&handle->h_mutex);
		return ANANAS_ERROR_OK;
	}

	/* Remove us from the thread handle queue, if necessary */
	if (handle->h_thread != NULL) {
		spinlock_lock(&handle->h_thread->t_lock);
		DQUEUE_REMOVE(&handle->h_thread->t_handles, handle);
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

	/* XXX Should we revoke all waiters too? */

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
handle_isvalid(struct HANDLE* handle, thread_t* t, int type)
{
	/* see if the handle resides in handlespace */
	if ((void*)handle < handle_memory_start || (void*)handle >= handle_memory_end)
		return ANANAS_ERROR(BAD_HANDLE);
	/* ensure the alignment is ok */
	if (((addr_t)handle % sizeof(struct HANDLE)) != 0)
		return ANANAS_ERROR(BAD_HANDLE);
	/* ensure handle is properly owned by the thread */
	if (handle->h_thread != t)
		return ANANAS_ERROR(BAD_HANDLE);
	/* if this is a handle reference, check the type of the handle we are refering to */
	struct HANDLE* desthandle = handle;
	if (handle->h_type == HANDLE_TYPE_REFERENCE)
		desthandle = handle->h_data.d_handle;
	if (type != HANDLE_TYPE_ANY && desthandle->h_type != type)
		return ANANAS_ERROR(BAD_HANDLE);
	/* ... yet unused handles are never valid */
	if (desthandle->h_type == HANDLE_TYPE_UNUSED)
		return ANANAS_ERROR(BAD_HANDLE);
	KASSERT(desthandle->h_refcount > 0, "invalid refcount %d", desthandle->h_refcount);
	return ANANAS_ERROR_OK;
}

errorcode_t
handle_clone_generic(thread_t* t, struct HANDLE* handle, struct HANDLE** out)
{
	struct HANDLE* newhandle;
	errorcode_t err = handle_alloc(handle->h_type, t, &newhandle);
	ANANAS_ERROR_RETURN(err);
	memcpy(&newhandle->h_data, &handle->h_data, sizeof(handle->h_data));
	*out = newhandle;
	return ANANAS_ERROR_OK;
}

errorcode_t
handle_clone(thread_t* t, struct HANDLE* handle, struct CLONE_OPTIONS* opts, struct HANDLE** out)
{
	errorcode_t err;

	mutex_lock(&handle->h_mutex);
	if (handle->h_flags & HANDLE_FLAG_TEARDOWN) {
		/* Handle is in the process of being removed; disallow the clone attempt */
		err = ANANAS_ERROR(BAD_HANDLE);
	} else if (handle->h_hops->hop_clone != NULL) {
		err = handle->h_hops->hop_clone(t, handle, opts, out);
	} else {
		err = ANANAS_ERROR(BAD_OPERATION);
	}
	mutex_unlock(&handle->h_mutex);
	return err;
}

errorcode_t
handle_wait(thread_t* thread, struct HANDLE* handle, handle_event_t* event, handle_event_result_t* result)
{
	TRACE(HANDLE, FUNC, "handle=%p, event=%p, thread=%p", handle, event, thread);

	mutex_lock(&handle->h_mutex);

	/* If this is a reference handle, deference it - waits must always be done using the real handle */
	if (handle->h_type == HANDLE_TYPE_REFERENCE) {
		struct HANDLE* h = handle->h_data.d_handle;
		mutex_unlock(&handle->h_mutex);
		mutex_lock(&h->h_mutex);
		handle = h;
		KASSERT(handle->h_type != HANDLE_TYPE_REFERENCE, "reference handle to reference?");
	}

	/*
	 * OK; first of all, see if the handle points to a zombie thread _which is no
	 * longer active_ (the latter is important, because it is impossible to call
	 * handle_signal() on the exact right moment - the thread still needs to be
	 * deactived by the scheduler!); if so, we should return immediately with the
	 * appropriate exit code - we can't guarantee that we'll enter the wait
	 * before the child dies...
	 *
	 * XXX need to handle mask
	 */
	if (handle->h_type == HANDLE_TYPE_THREAD &&
		  THREAD_IS_ZOMBIE(handle->h_data.d_thread) /* it's a zombie... */ &&
	    !THREAD_IS_ACTIVE(handle->h_data.d_thread) /* ...and done being scheduled */) {
		/* Need to replicate what thread_exit() does here... */
		*event = THREAD_EVENT_EXIT;
		*result = handle->h_data.d_thread->t_terminate_info;
		mutex_unlock(&handle->h_mutex);
		TRACE(HANDLE, INFO, "done, thread already gone");
		return ANANAS_ERROR_OK;
	}

	/* schedule the wait as usual */
	int waiter_id = 0;
	for (; waiter_id <  HANDLE_MAX_WAITERS; waiter_id++)
		if (handle->h_waiters[waiter_id].hw_thread == NULL)
			break;
	KASSERT(waiter_id < HANDLE_MAX_WAITERS, "FIXME: no waiter handles available");
	handle->h_waiters[waiter_id].hw_thread = thread;
	handle->h_waiters[waiter_id].hw_event_mask = *event;
	handle->h_waiters[waiter_id].hw_event_reported = 0;
	mutex_unlock(&handle->h_mutex);

	/* Now, we wait - handle_signal will wake us when the time comes */
	sem_wait(&handle->h_waiters[waiter_id].hw_sem);

	/* Obtain the result */
	mutex_lock(&handle->h_mutex);
	*result = handle->h_waiters[waiter_id].hw_result;
	*event = handle->h_waiters[waiter_id].hw_event_reported;
	/* And free the waiter slot */
	handle->h_waiters[waiter_id].hw_thread = NULL;
	mutex_unlock(&handle->h_mutex);

	/*
	 * If we are being woken up for a thread, we must wait until it is in the
	 * 'inactive zombie' state. The reason is that handle_signal() must be called
	 * from the terminating thread, which will still need to schedule something
	 * else - we must wait for this to be the case.
	 */
	if (handle->h_type == HANDLE_TYPE_THREAD /* XXX check event type for exiting */) {
		while (THREAD_IS_ACTIVE(handle->h_data.d_thread) || !THREAD_IS_ZOMBIE(handle->h_data.d_thread))
			schedule();
	}
	TRACE(HANDLE, INFO, "t=%p, done", thread);
	return ANANAS_ERROR_OK;
}

void
handle_signal(struct HANDLE* handle, handle_event_t event, handle_event_result_t result)
{
	TRACE(HANDLE, FUNC, "handle=%p, event=%p, result=0x%x", handle, event, result);
	mutex_lock(&handle->h_mutex);
	KASSERT(handle->h_type != HANDLE_TYPE_UNUSED, "cannot signal unused handle %p", handle);
	KASSERT(handle->h_type != HANDLE_TYPE_REFERENCE, "cannot signal reference handle %p", handle);
	for (int waiter_id = 0; waiter_id <  HANDLE_MAX_WAITERS; waiter_id++) {
		if (handle->h_waiters[waiter_id].hw_thread == NULL)
			continue;
		if ((event != 0) && ((handle->h_waiters[waiter_id].hw_event_mask != HANDLE_EVENT_ANY) && (handle->h_waiters[waiter_id].hw_event_mask & event) == 0))
			continue;

		handle->h_waiters[waiter_id].hw_event_reported = event;
		handle->h_waiters[waiter_id].hw_result = result;
		sem_signal(&handle->h_waiters[waiter_id].hw_sem);
	}
	mutex_unlock(&handle->h_mutex);
	TRACE(HANDLE, INFO, "done");
}

errorcode_t
handle_set_owner(struct THREAD* t, struct HANDLE* handle, struct HANDLE* owner)
{
	/* Fetch the thread */
	errorcode_t err = handle_isvalid(owner, t, HANDLE_TYPE_THREAD);
	ANANAS_ERROR_RETURN(err);
	thread_t* new_thread;
	if (owner->h_type == HANDLE_TYPE_REFERENCE)
		new_thread = owner->h_data.d_handle->h_data.d_thread;
	else
		new_thread = owner->h_data.d_thread;

	/* XXX We should check the relationship between the current and new thread */

	/*
	 * Setting the owner is only allowed if the refcount is exactly one; this
	 * avoids having to walk through all references to this handle to see if
	 * we should be allowed to change the owner.
	 */
	if (handle->h_refcount != 1)
		return ANANAS_ERROR(BAD_OPERATION);

	/* Remove the thread from the old thread's handles */
	thread_t* old_thread = handle->h_thread;
	spinlock_lock(&old_thread->t_lock);
	DQUEUE_REMOVE(&old_thread->t_handles, handle);
	spinlock_unlock(&old_thread->t_lock);

	/* And hook it up the new thread's handles */
	spinlock_lock(&new_thread->t_lock);
	DQUEUE_ADD_TAIL(&new_thread->t_handles, handle);
	handle->h_thread = new_thread;
	spinlock_unlock(&new_thread->t_lock);
	return ANANAS_ERROR_OK;
}

errorcode_t
handle_read(struct HANDLE* handle, void* buffer, size_t* size)
{
	errorcode_t err;

	mutex_lock(&handle->h_mutex);
	if (handle->h_hops->hop_read != NULL) {
		err = handle->h_hops->hop_read(handle->h_thread, handle, buffer, size);
	} else {
		err = ANANAS_ERROR(BAD_OPERATION);
	}
	mutex_unlock(&handle->h_mutex);
	return err;
}

errorcode_t
handle_write(struct HANDLE* handle, const void* buffer, size_t* size)
{
	errorcode_t err;

	mutex_lock(&handle->h_mutex);
	if (handle->h_hops->hop_write != NULL) {
		err = handle->h_hops->hop_write(handle->h_thread, handle, buffer, size);
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
void
kdb_cmd_handle(int num_args, char** arg)
{
	if (num_args != 2) {
		kprintf("need an argument\n");
		return;
	}

	char* ptr;
	addr_t addr = (addr_t)strtoul(arg[1], &ptr, 16);
	if (*ptr != '\0') {
		kprintf("parse error at '%s'\n", ptr);
		return;
	}

	struct HANDLE* handle = (void*)addr;
	kprintf("type          : %u\n", handle->h_type);
	kprintf("flags         : %u\n", handle->h_flags);
	kprintf("refcount      : %d\n", handle->h_refcount);
	kprintf("owner thread  : 0x%x\n", handle->h_thread);
	kprintf("waiters:\n");
	for (unsigned int i = 0; i < HANDLE_MAX_WAITERS; i++) {
		if (handle->h_waiters[i].hw_thread == NULL)
			continue;
		kprintf("   wait thread    : 0x%x\n", handle->h_waiters[i].hw_thread);
		kprintf("   wait event     : %u\n", handle->h_waiters[i].hw_event);
		kprintf("   event mask     : %u\n", handle->h_waiters[i].hw_event_mask);
		kprintf("   event reported : %u\n", handle->h_waiters[i].hw_event_reported);
		kprintf("   result         : %u\n", handle->h_waiters[i].hw_result);
	}

	switch(handle->h_type) {
		case HANDLE_TYPE_FILE: {
			kprintf("file handle specifics:\n");
			kprintf("   offset         : %u\n", handle->h_data.d_vfs_file.f_offset); /* XXXSIZE */
			kprintf("   inode          : 0x%x\n", handle->h_data.d_vfs_file.f_inode);
			kprintf("   device         : 0x%x\n", handle->h_data.d_vfs_file.f_device);
			break;
		}
		case HANDLE_TYPE_THREAD: {
			kprintf("thread handle specifics:\n");
			kprintf("   thread         : 0x%x\n", handle->h_data.d_thread);
			break;
		}
		case HANDLE_TYPE_MEMORY: {
			kprintf("memory handle specifics:\n");
			kprintf("   address        : 0x%x\n", handle->h_data.d_memory.hmi_addr);
			kprintf("   size           : %u\n", handle->h_data.d_memory.hmi_length);
			break;
		}
	}
}
#endif

/* vim:set ts=2 sw=2: */
