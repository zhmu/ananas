#ifndef __SYS_HANDLE_H__
#define __SYS_HANDLE_H__

#include <ananas/lock.h>
#include <ananas/cbuffer.h>
#include <ananas/dqueue.h>
#include <ananas/lock.h>
#include <ananas/vfs/types.h>

typedef unsigned int handle_event_t;
typedef unsigned int handle_event_result_t;

#define HANDLE_TYPE_ANY		-1
#define HANDLE_TYPE_UNUSED	0
#define HANDLE_TYPE_FILE	1
#define HANDLE_TYPE_THREAD	2
#define HANDLE_TYPE_MEMORY	3
#define HANDLE_TYPE_REFERENCE	4
#define HANDLE_TYPE_PIPE	5

#define HANDLE_EVENT_ANY	0
#define HANDLE_EVENT_READ	1
#define HANDLE_EVENT_WRITE	2

#define HANDLE_VALUE_INVALID	0
#define HANDLE_MAX_WAITERS	10	/* XXX should be any limit */

#define HANDLE_FLAG_TEARDOWN	1	/* if set, handle is going away and can't be reffed anymore */

struct THREAD;
struct HANDLE_OPS;

/* Waiters are threads waiting for an event on a thread */
struct HANDLE_WAITER {
	struct THREAD* hw_thread;
	semaphore_t hw_sem;
	int hw_event;
	handle_event_t hw_event_mask;
	handle_event_t hw_event_reported;
	handle_event_result_t hw_result;
};

struct HANDLE_MEMORY_INFO {
	struct VM_AREA* hmi_vmarea;
};

struct HANDLE_PIPE_BUFFER {
	mutex_t hpb_mutex;
	refcount_t hpb_read_count;
	refcount_t hpb_write_count;
	CBUFFER_FIELDS;
	char hpb_buffer[1];
};

struct HANDLE_PIPE_INFO {
	int hpi_flags;
#define HPI_FLAG_READ	0x0001
#define HPI_FLAG_WRITE	0x0002
	struct HANDLE_PIPE_BUFFER* hpi_buffer;
	/*
	 * All HANDLE_PIPE_INFO structs that belong to the same buffer are
	 * in a circular linked list - this is done so that they can be
	 * woken up once any of them is written/read/closed. This also
	 * means we need the handle itself. XXX KLUDGE
	 */
	struct HANDLE_PIPE_INFO* hpi_next;
	struct HANDLE* hpi_handle;
};

struct HANDLE_REF_INFO {
	struct HANDLE* ref_handle;
	thread_t* ref_thread;
	handleindex_t ref_index;
};

struct HANDLE {
	int h_type;				/* one of HANDLE_TYPE_... */
	int h_flags;				/* flags */
	struct THREAD* h_thread;		/* owning thread */
	mutex_t h_mutex;			/* mutex guarding the handle */
	refcount_t h_refcount;			/* reference count */
	struct HANDLE_OPS* h_hops;		/* handle operations */
	DQUEUE_FIELDS(struct HANDLE);		/* used for the queue structure */

	/* Waiters are those who are waiting on this handle */
	struct HANDLE_WAITER h_waiters[HANDLE_MAX_WAITERS];
	union {
		struct VFS_FILE d_vfs_file;
		struct THREAD*  d_thread;
		struct HANDLE_MEMORY_INFO d_memory;
		struct HANDLE_PIPE_INFO d_pipe;
		struct HANDLE_REF_INFO d_ref;
	} h_data;
};

DQUEUE_DEFINE(HANDLE_QUEUE, struct HANDLE);

/*
 * Handle operations map almost directly to the syscalls invoked on them.
 */
struct OPEN_OPTIONS;
struct CREATE_OPTIONS;
struct SUMMON_OPTIONS;
struct CLONE_OPTIONS;
typedef errorcode_t (*handle_read_fn)(thread_t* thread, handleindex_t index, struct HANDLE* handle, void* buf, size_t* len);
typedef errorcode_t (*handle_write_fn)(thread_t* thread, handleindex_t index, struct HANDLE* handle, const void* buf, size_t* len);
typedef errorcode_t (*handle_open_fn)(thread_t* thread, handleindex_t index, struct HANDLE* result, struct OPEN_OPTIONS* opts);
typedef errorcode_t (*handle_free_fn)(thread_t* thread, struct HANDLE* handle);
typedef errorcode_t (*handle_unlink_fn)(thread_t* thread, handleindex_t index, struct HANDLE* handle);
typedef errorcode_t (*handle_create_fn)(thread_t* thread, handleindex_t index, struct HANDLE* handle, struct CREATE_OPTIONS* opts);
typedef errorcode_t (*handle_control_fn)(thread_t* thread, handleindex_t index, struct HANDLE* handle, unsigned int op, void* arg, size_t len);
typedef errorcode_t (*handle_clone_fn)(thread_t* thread_in, handleindex_t index, struct HANDLE* handle, struct CLONE_OPTIONS* opts, thread_t* thread_out, struct HANDLE** handle_out, handleindex_t* index_out);
typedef errorcode_t (*handle_summon_fn)(thread_t* thread, struct HANDLE* handle, struct SUMMON_OPTIONS* opts, struct HANDLE** handle_out, handleindex_t* index_out);

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
errorcode_t handle_alloc(int type, thread_t* t, handleindex_t index_from, struct HANDLE** handle_out, handleindex_t* index_out);
errorcode_t handle_free(struct HANDLE* h);
errorcode_t handle_free_byindex(thread_t* t, handleindex_t index);
errorcode_t handle_lookup(thread_t* t, handleindex_t index, int type, struct HANDLE** handle_out);
errorcode_t handle_clone(thread_t* t_in, handleindex_t index, struct CLONE_OPTIONS* opts, thread_t* thread_out, struct HANDLE** handle_out, handleindex_t* index_out);
errorcode_t handle_set_owner(thread_t* t, handleindex_t index_in, handleindex_t owner_thread_in, handleindex_t* index_out);
errorcode_t handle_create_ref(thread_t* thread_in, handleindex_t index_in, thread_t* thread_out, struct HANDLE** handle_out, handleindex_t* index_out);

errorcode_t handle_wait(thread_t* t, handleindex_t index, handle_event_t* event, handle_event_result_t* h);
void handle_signal(struct HANDLE* handle, handle_event_t event, handle_event_result_t result);

/* Only to be used from handle implementation code */
errorcode_t handle_clone_generic(struct HANDLE* handle, thread_t* thread_out, struct HANDLE** out, handleindex_t* index);
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
