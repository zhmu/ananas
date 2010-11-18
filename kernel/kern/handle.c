#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/handle.h>
#include <ananas/lib.h>
#include <ananas/lock.h>
#include <ananas/mm.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/thread.h>

TRACE_SETUP;

#define NUM_HANDLES 1000 /* XXX shouldn't be static */
#undef DEBUG_CLONE
#undef DEBUG_WAIT
#undef DEBUG_SIGNAL

static struct HANDLE* handle_pool = NULL;
static struct SPINLOCK spl_handlepool = { 0 };
static void *handle_memory_start = NULL;
static void *handle_memory_end = NULL;

void
handle_init()
{
	handle_pool = kmalloc(sizeof(struct HANDLE) * (NUM_HANDLES + 1));
	/*
	 * Ensure handle pool is aligned; this makes checking for valid handles
	 * easier.
	 */
	handle_pool = (struct HANDLE*)((addr_t)handle_pool + (sizeof(struct HANDLE) - (addr_t)handle_pool % sizeof(struct HANDLE)));
	memset(handle_pool, 0, sizeof(struct HANDLE) * NUM_HANDLES);

	for (unsigned int i = 0; i < NUM_HANDLES; i++) {
		if (i < NUM_HANDLES - 1)
			handle_pool[i].next = &handle_pool[i + 1];
		if (i > 0)
			handle_pool[i].prev = &handle_pool[i - 1];
		spinlock_init(&handle_pool[i].spl_handle);
	}

	handle_memory_start = handle_pool;
	handle_memory_end   = (char*)handle_pool + sizeof(struct HANDLE) * NUM_HANDLES;
}

errorcode_t
handle_alloc(int type, struct THREAD* t, struct HANDLE** out)
{
	KASSERT(handle_pool != NULL, "handle_alloc() without pool");

	/* Grab a handle from the pool */
	spinlock_lock(&spl_handlepool);
	struct HANDLE* handle = handle_pool;
	if (handle == NULL) {
		/* XXX should we wait for a new handle, or just give an error? */
		panic("out of handles");
	}
	if (handle->next != NULL)
		handle->next->prev = NULL;
	handle_pool = handle->next;
	spinlock_unlock(&spl_handlepool);

	/* Sanity checks */
	KASSERT(handle->type == HANDLE_TYPE_UNUSED, "handle from pool must be unused");

	/* Hook the handle to the thread */
	handle->type = type;
	handle->thread = t;
	handle->prev = NULL;

	if (t != NULL) {
		/* XXX lock thread */
		handle->next = t->handle;
		if (t->handle != NULL)
			t->handle->prev = handle;
	t->handle = handle;
		/* XXX unlock thread */
	} else {
		handle->next = NULL;
	}

	KASSERT(((addr_t)handle % sizeof(struct HANDLE)) == 0, "handle address %p is not 0x%x byte aligned", (addr_t)handle, sizeof(struct HANDLE));
	*out = handle;
	return ANANAS_ERROR_OK;
}

errorcode_t
handle_free(struct HANDLE* handle)
{
	KASSERT(handle->type != HANDLE_TYPE_UNUSED, "freeing free handle");

	/* Remove us from the handle context */
	if (handle->prev != NULL)
		handle->prev->next = handle->next;
	if (handle->next != NULL)
		handle->next->prev = handle->prev;

	/* XXX lock thread */
	/* Remove us from the thread's handle context */
	if (handle->thread != NULL && handle->thread->handle == handle) {
		handle->thread->handle = handle->next;
	}

	/* Clear the handle */
	memset(&handle->data, 0, sizeof(handle->data));
	handle->type = HANDLE_TYPE_UNUSED; /* just to ensure the value matches */
	handle->thread = NULL;
	handle->prev = NULL;

	/* Hand it back to the the pool */
	spinlock_lock(&spl_handlepool);
	handle->next = handle_pool;
	if (handle_pool != NULL)
		handle_pool->prev = handle;
	handle_pool = handle;
	spinlock_unlock(&spl_handlepool);
	return ANANAS_ERROR_OK;
}

errorcode_t
handle_isvalid(struct HANDLE* handle, struct THREAD* t, int type)
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
handle_clone(struct HANDLE* handle, struct HANDLE** out)
{
	errorcode_t err;

	spinlock_lock(&handle->spl_handle);
	switch(handle->type) {
		case HANDLE_TYPE_THREAD: {
			struct THREAD* newthread;
			err = thread_clone(handle->thread, 0, &newthread);
			spinlock_unlock(&handle->spl_handle);
			ANANAS_ERROR_RETURN(err);
#ifdef DEBUG_CLONE
			kprintf("newthread handle = %x\n", newthread->thread_handle);
#endif
			*out = newthread->thread_handle;
			return ANANAS_ERROR_OK;
		}
		default: {
			struct HANDLE* newhandle;
			err = handle_alloc(handle->type, handle->thread, &newhandle);
			ANANAS_ERROR_RETURN(err);
			memcpy(&newhandle->data, &handle->data, sizeof(handle->data));
			spinlock_unlock(&handle->spl_handle);
			*out = newhandle;
			return ANANAS_ERROR_OK;
		}
	}
	/* NOTREACHED */
}

errorcode_t
handle_wait(struct THREAD* thread, struct HANDLE* handle, handle_event_t* event, handle_event_result_t* result)
{
#ifdef DEBUG_WAIT
	kprintf("handle_wait(t=%p): handle=%p, event=%p\n", thread, handle, event);
#endif

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
#ifdef DEBUG_WAIT
		kprintf("handle_wait(t=%p): done, thread already gone\n", thread);
#endif
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

#ifdef DEBUG_WAIT
	kprintf("handle_wait(t=%p): done\n", thread);
#endif
	return ANANAS_ERROR_OK;
}

void
handle_signal(struct HANDLE* handle, handle_event_t event, handle_event_result_t result)
{
#ifdef DEBUG_SIGNAL
	kprintf("handle_signal(): handle=%p, event=%p, result=0x%x\n", handle, event, result);
#endif
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
#ifdef DEBUG_SIGNAL
	kprintf("handle_signal(): done\n");
#endif
}

/* vim:set ts=2 sw=2: */
