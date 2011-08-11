#ifndef __SYS_HANDLE_H__
#define __SYS_HANDLE_H__

#include <ananas/lock.h>
#include <ananas/dqueue.h>
#include <ananas/vfs/types.h>

typedef unsigned int handle_event_t;
typedef unsigned int handle_event_result_t;

#define HANDLE_TYPE_ANY		-1
#define HANDLE_TYPE_UNUSED	0
#define HANDLE_TYPE_FILE	1
#define HANDLE_TYPE_THREAD	2
#define HANDLE_TYPE_MEMORY	3

#define HANDLE_EVENT_ANY	0

#define HANDLE_VALUE_INVALID	0
#define HANDLE_MAX_WAITERS	10	/* XXX should be any limit */

struct THREAD;
struct HANDLE_OPS;

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
	spinlock_t spl_handle;			/* spinlock guarding the handle */
	struct HANDLE_OPS* hops;		/* handle operations */
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

/*
 * Handle operations map almost directly to the syscalls invoked on them.
 */
struct OPEN_OPTIONS;
struct CREATE_OPTIONS;
struct SUMMON_OPTIONS;
typedef errorcode_t (*handle_read_fn)(thread_t* thread, struct HANDLE* handle, void* buf, size_t* len);
typedef errorcode_t (*handle_write_fn)(thread_t* thread, struct HANDLE* handle, const void* buf, size_t* len);
typedef errorcode_t (*handle_open_fn)(thread_t* thread, struct HANDLE* result, struct OPEN_OPTIONS* opts);
typedef errorcode_t (*handle_free_fn)(thread_t* thread, struct HANDLE* handle);
typedef errorcode_t (*handle_unlink_fn)(thread_t* thread, struct HANDLE* handle);
typedef errorcode_t (*handle_create_fn)(thread_t* thread, struct HANDLE* handle, struct CREATE_OPTIONS* opts);
typedef errorcode_t (*handle_control_fn)(thread_t* thread, struct HANDLE* handle, unsigned int op, void* arg, size_t len);
typedef errorcode_t (*handle_clone_fn)(thread_t* thread, struct HANDLE* handle, struct HANDLE** out);
typedef errorcode_t (*handle_summon_fn)(thread_t* thread, struct HANDLE* handle, struct SUMMON_OPTIONS* opts, struct HANDLE** out);

struct HANDLE_OPS {
	handle_read_fn hop_read;
	handle_write_fn hop_write;
	handle_open_fn hop_open;
	handle_free_fn hop_free;
	handle_unlink_fn hop_unlink;
	handle_create_fn hop_create;
	handle_control_fn hop_control;
	handle_clone_fn hop_clone;
	handle_summon_fn hop_summon;
};

/* Registration of handle types */
struct HANDLE_TYPE {
	const char* ht_name;
	int ht_id;
	struct HANDLE_OPS* ht_hops;
	DQUEUE_FIELDS(struct HANDLE_TYPE);
};
DQUEUE_DEFINE(HANDLE_TYPES, struct HANDLE_TYPE);

void handle_init();
errorcode_t handle_alloc(int type, struct THREAD* t, struct HANDLE** out);
errorcode_t handle_destroy(struct HANDLE* handle, int free_resources);
errorcode_t handle_isvalid(struct HANDLE* handle, struct THREAD* t, int type);
errorcode_t handle_clone(struct THREAD* t, struct HANDLE* in, struct HANDLE** out);
errorcode_t handle_set_owner(struct HANDLE* handle, struct HANDLE* owner);

errorcode_t handle_wait(struct THREAD* thread, struct HANDLE* handle, handle_event_t* event, handle_event_result_t* h);
void handle_signal(struct HANDLE* handle, handle_event_t event, handle_event_result_t result);

#define handle_free(handle) \
	handle_destroy((handle), 1)

/* Only to be used from handle implementation code */
errorcode_t handle_clone_generic(thread_t* t, struct HANDLE* handle, struct HANDLE** out);
errorcode_t handle_register_type(struct HANDLE_TYPE* ht);
errorcode_t handle_unregister_type(struct HANDLE_TYPE* ht);

#define HANDLE_TYPE(id, name, ops) \
	static struct HANDLE_TYPE ht_##id; \
	static errorcode_t register_##id() { \
		return handle_register_type(&ht_##id); \
	}; \
	static errorcode_t unregister_##id() { \
		return handle_unregister_type(&ht_##id); \
	}; \
	INIT_FUNCTION(register_##id, SUBSYSTEM_HANDLE, ORDER_SECOND); \
	EXIT_FUNCTION(unregister_##id); \
	static struct HANDLE_TYPE ht_##id = { \
		.ht_name = name, \
		.ht_id = id, \
		.ht_hops = &ops \
	}
	
#endif /* __SYS_HANDLE_H__ */
