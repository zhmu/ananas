#ifndef __SYS_HANDLE_H__
#define __SYS_HANDLE_H__

#include <ananas/lock.h>
#include <ananas/dqueue.h>
#include <ananas/vfs.h>

typedef unsigned int handle_event_t;
typedef unsigned int handle_event_result_t;

#define HANDLE_TYPE_ANY		-1
#define HANDLE_TYPE_UNUSED	0
#define HANDLE_TYPE_FILE	1
#define HANDLE_TYPE_THREAD	2
#define HANDLE_TYPE_MEMORY	3

#define HANDLE_EVENT_ANY	0

#define HANDLE_VALUE_INVALID	0
#define HANDLE_MAX_WAITERS	10	/* XXX should be any limit*/

struct THREAD;
struct VFS_FILE;

/* Waiters are threads waiting for an event on a thread */
struct HANDLE_WAITER {
	struct THREAD* thread;
	int event;
	handle_event_t event_mask;
	handle_event_t event_reported;
	handle_event_result_t result;
};

struct HANDLE_MEMORY_INFO {
	struct THREAD_MAPPING* mapping;
	void* addr;
	size_t length;
};

struct HANDLE {
	int type;
	struct THREAD* thread;			/* owning thread */
	struct SPINLOCK spl_handle;		/* spinlock guarding the handle */
	DQUEUE_FIELDS(struct HANDLE);		/* used for the queue structure */

	/* Waiters are those who are waiting on this handle */
	struct HANDLE_WAITER waiters[HANDLE_MAX_WAITERS];
	union {
		struct VFS_FILE vfs_file;
		struct THREAD*  thread;
		struct HANDLE_MEMORY_INFO memory;
	} data;
};

DQUEUE_DEFINE(HANDLE_QUEUE, struct HANDLE);

void handle_init();
errorcode_t handle_alloc(int type, struct THREAD* t, struct HANDLE** out);
errorcode_t handle_destroy(struct HANDLE* handle, int free_resources);
errorcode_t handle_isvalid(struct HANDLE* handle, struct THREAD* t, int type);
errorcode_t handle_clone(struct THREAD* t, struct HANDLE* in, struct HANDLE** out);

errorcode_t handle_wait(struct THREAD* thread, struct HANDLE* handle, handle_event_t* event, handle_event_result_t* h);
void handle_signal(struct HANDLE* handle, handle_event_t event, handle_event_result_t result);

#define handle_free(handle) \
	handle_destroy((handle), 1)

#endif /* __SYS_HANDLE_H__ */
