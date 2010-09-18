#ifndef __SYS_HANDLE_H__
#define __SYS_HANDLE_H__

#include <ananas/lock.h>
#include <ananas/vfs.h>

typedef unsigned int handle_event_t;
typedef unsigned int handle_event_result_t;

#define HANDLE_TYPE_ANY		-1
#define HANDLE_TYPE_UNUSED	0
#define HANDLE_TYPE_FILE	1
#define HANDLE_TYPE_THREAD	2

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

struct HANDLE {
	int type;
	struct THREAD* thread;			/* owning thread */
	struct HANDLE *prev, *next;		/* chain */
	struct SPINLOCK spl_handle;		/* spinlock guarding the handle */

	/* Waiters are those who are waiting on this handle */
	struct HANDLE_WAITER waiters[HANDLE_MAX_WAITERS];
	union {
		struct VFS_FILE vfs_file;
		struct THREAD*  thread;
	} data;
};

void handle_init();
struct HANDLE* handle_alloc(int type, struct THREAD* t);
void handle_free(struct HANDLE* handle);
int handle_isvalid(struct HANDLE* handle, struct THREAD* t, int type);
struct HANDLE* handle_clone(struct HANDLE* handle);

handle_event_result_t handle_wait(struct THREAD* thread, struct HANDLE* handle, handle_event_t* mask);
void handle_signal(struct HANDLE* handle, handle_event_t event, handle_event_result_t result);

#endif /* __SYS_HANDLE_H__ */
