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

#define NUM_HANDLES 1000 /* XXX shouldn't be static */

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
		spinlock_init(&h->spl_handle);
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
	KASSERT(handle->type == HANDLE_TYPE_UNUSED, "handle from pool must be unused");

	/* Hook the handle to the thread */
	handle->type = type;
	handle->thread = t;
	handle->hops = htype->ht_hops;

	/* Hook the handle to the thread queue */
	spinlock_lock(&t->spl_thread);
	DQUEUE_ADD_TAIL(&t->handles, handle);
	spinlock_unlock(&t->spl_thread);

	KASSERT(((addr_t)handle % sizeof(struct HANDLE)) == 0, "handle address %p is not 0x%x byte aligned", (addr_t)handle, sizeof(struct HANDLE));
	*out = handle;
	TRACE(HANDLE, INFO, "thread=%p, type=%u => handle=%p", t, type, handle);
	return ANANAS_ERROR_OK;
}

errorcode_t
handle_destroy(struct HANDLE* handle, int free_resources)
{
	KASSERT(handle->type != HANDLE_TYPE_UNUSED, "freeing free handle");
	TRACE(HANDLE, FUNC, "handle=%p, free_resource=%u", handle, free_resources);

	/* Remove us from the thread handle queue, if necessary */
	if (handle->thread != NULL) {
		spinlock_lock(&handle->thread->spl_thread);
		DQUEUE_REMOVE(&handle->thread->handles, handle);
		spinlock_unlock(&handle->thread->spl_thread);
	}

	/* If we need to free our resources, do so */
	errorcode_t err = ANANAS_ERROR_OK;
	if (free_resources) {
		/*
		 * If the handle has a specific free function, call it - otherwise assume
		 * no special action is needed.
		 */
		if (handle->hops->hop_free != NULL) {
			err = handle->hops->hop_free(handle->thread, handle);
			ANANAS_ERROR_RETURN(err);
		}
	}

	/* Clear the handle */
	memset(&handle->data, 0, sizeof(handle->data));
	handle->type = HANDLE_TYPE_UNUSED; /* just to ensure the value matches */
	handle->thread = NULL;

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
	/* XXX does not work with inherited handles yet XXX */
#if 0
	if (handle->thread != t)
		return ANANAS_ERROR(BAD_HANDLE);
#endif
	/* ensure handle type is correct */
	if (type != HANDLE_TYPE_ANY && handle->type != type)
		return ANANAS_ERROR(BAD_HANDLE);
	/* ... yet unused handles are never valid */
	if (handle->type == HANDLE_TYPE_UNUSED)
		return ANANAS_ERROR(BAD_HANDLE);
	return ANANAS_ERROR_OK;
}

errorcode_t
handle_clone_generic(thread_t* t, struct HANDLE* handle, struct HANDLE** out)
{
	struct HANDLE* newhandle;
	errorcode_t err = handle_alloc(handle->type, t, &newhandle);
	ANANAS_ERROR_RETURN(err);
	memcpy(&newhandle->data, &handle->data, sizeof(handle->data));
	*out = newhandle;
	return ANANAS_ERROR_OK;
}

errorcode_t
handle_clone(thread_t* t, struct HANDLE* handle, struct HANDLE** out)
{
	errorcode_t err;

	spinlock_lock(&handle->spl_handle);
	if (handle->hops->hop_clone != NULL) {
		err = handle->hops->hop_clone(t, handle, out);
	} else {
		err = ANANAS_ERROR(BAD_OPERATION);
	}
	spinlock_unlock(&handle->spl_handle);
	return err;
}

errorcode_t
handle_wait(thread_t* thread, struct HANDLE* handle, handle_event_t* event, handle_event_result_t* result)
{
	TRACE(HANDLE, FUNC, "handle=%p, event=%p, thread=%p", handle, event, thread);

	spinlock_lock(&handle->spl_handle);

	/*
	 * OK; first of all, see if the handle points to a terminating thread; if so, we
	 * should return immediately with the appropriate exit code. The reason is that
	 * there's no control, should a child die, that it dies before the parent got
	 * a chance to do this wait...
	 *
	 * XXX need to handle mask
	 */
	if ((handle->type == HANDLE_TYPE_THREAD) &&
	    (handle->data.thread->flags & THREAD_FLAG_TERMINATING) != 0) {
		/* Need to replicate what thread_exit() does here... */
		*event = THREAD_EVENT_EXIT;
		*result = handle->data.thread->terminate_info;
		spinlock_unlock(&handle->spl_handle);
		TRACE(HANDLE, INFO, "done, thread already gone", thread);
		return ANANAS_ERROR_OK;
	}

	/* schedule the wait as usual */
	int waiter_id = 0;
	for (; waiter_id <  HANDLE_MAX_WAITERS; waiter_id++)
		if (handle->waiters[waiter_id].thread == NULL)
			break;
	KASSERT(waiter_id < HANDLE_MAX_WAITERS, "FIXME: no waiter handles available");
	handle->waiters[waiter_id].thread = thread;
	handle->waiters[waiter_id].event_mask = *event;
	handle->waiters[waiter_id].event_reported = 0;
	spinlock_unlock(&handle->spl_handle);

	/* Now, we wait - handle_signal will wake us when the time comes */
	thread_suspend(thread);
	reschedule();

	/* Obtain the result */
	spinlock_lock(&handle->spl_handle);
	*result = handle->waiters[waiter_id].result;
	*event = handle->waiters[waiter_id].event_reported;
	/* And free the waiter slot */
	handle->waiters[waiter_id].thread = NULL;
	spinlock_unlock(&handle->spl_handle);

	TRACE(HANDLE, INFO, "t=%p, done", thread);
	return ANANAS_ERROR_OK;
}

void
handle_signal(struct HANDLE* handle, handle_event_t event, handle_event_result_t result)
{
	TRACE(HANDLE, FUNC, "handle=%p, event=%p, result=0x%x", handle, event, result);
	spinlock_lock(&handle->spl_handle);
	for (int waiter_id = 0; waiter_id <  HANDLE_MAX_WAITERS; waiter_id++) {
		if (handle->waiters[waiter_id].thread == NULL)
			continue;
		if ((event != 0) && ((handle->waiters[waiter_id].event_mask != HANDLE_EVENT_ANY) && (handle->waiters[waiter_id].event_mask & event) == 0))
			continue;

		handle->waiters[waiter_id].event_reported = event;
		handle->waiters[waiter_id].result = result;
		thread_resume(handle->waiters[waiter_id].thread);
	}
	spinlock_unlock(&handle->spl_handle);
	TRACE(HANDLE, INFO, "done");
}

errorcode_t
handle_set_owner(struct HANDLE* handle, struct HANDLE* owner)
{
	/* Fetch the thread */
	errorcode_t err = handle_isvalid(owner, NULL, HANDLE_TYPE_THREAD);
	ANANAS_ERROR_RETURN(err);
	thread_t* new_thread = owner->data.thread;

	/* XXX We should check the relationship between the current and new thread */

	/* Remove the thread from the old thread's handles */
	thread_t* old_thread = handle->thread;
	spinlock_lock(&old_thread->spl_thread);
	DQUEUE_REMOVE(&old_thread->handles, handle);
	spinlock_unlock(&old_thread->spl_thread);

	/* And hook it up the new thread's handles */
	spinlock_lock(&new_thread->spl_thread);
	DQUEUE_ADD_TAIL(&new_thread->handles, handle);
	spinlock_unlock(&new_thread->spl_thread);
	return ANANAS_ERROR_OK;
}

errorcode_t
handle_read(struct HANDLE* handle, void* buffer, size_t* size)
{
	errorcode_t err;

	spinlock_lock(&handle->spl_handle);
	if (handle->hops->hop_read != NULL) {
		err = handle->hops->hop_read(handle->thread, handle, buffer, size);
	} else {
		err = ANANAS_ERROR(BAD_OPERATION);
	}
	spinlock_unlock(&handle->spl_handle);
	return err;
}

errorcode_t
handle_write(struct HANDLE* handle, const void* buffer, size_t* size)
{
	errorcode_t err;

	spinlock_lock(&handle->spl_handle);
	if (handle->hops->hop_write != NULL) {
		err = handle->hops->hop_write(handle->thread, handle, buffer, size);
	} else {
		err = ANANAS_ERROR(BAD_OPERATION);
	}
	spinlock_unlock(&handle->spl_handle);
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

#ifdef KDB
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
	kprintf("type          : %u\n", handle->type);
	kprintf("owner thread  : 0x%x\n", handle->thread);
	kprintf("waiters:\n");
	for (unsigned int i = 0; i < HANDLE_MAX_WAITERS; i++) {
		if (handle->waiters[i].thread == NULL)
			continue;
		kprintf("   wait thread    : 0x%x\n", handle->waiters[i].thread);
		kprintf("   wait event     : %u\n", handle->waiters[i].event);
		kprintf("   event mask     : %u\n", handle->waiters[i].event_mask);
		kprintf("   event reported : %u\n", handle->waiters[i].event_reported);
		kprintf("   result         : %u\n", handle->waiters[i].result);
	}

	switch(handle->type) {
		case HANDLE_TYPE_FILE: {
			kprintf("file handle specifics:\n");
			kprintf("   offset         : %u\n", handle->data.vfs_file.f_offset); /* XXXSIZE */
			kprintf("   inode          : 0x%x\n", handle->data.vfs_file.f_inode);
			kprintf("   device         : 0x%x\n", handle->data.vfs_file.f_device);
			break;
		}
		case HANDLE_TYPE_THREAD: {
			kprintf("thread handle specifics:\n");
			kprintf("   thread         : 0x%x\n", handle->data.thread);
			break;
		}
		case HANDLE_TYPE_MEMORY: {
			kprintf("memory handle specifics:\n");
			kprintf("   address        : 0x%x\n", handle->data.memory.addr);
			kprintf("   size           : %u\n", handle->data.memory.length);
			break;
		}
	}
}
#endif

/* vim:set ts=2 sw=2: */
