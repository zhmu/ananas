#ifndef __ANANAS_EXEC_H__
#define __ANANAS_EXEC_H__

#include <ananas/types.h>
#include <ananas/dqueue.h>
#include <ananas/error.h>
#include <ananas/init.h>

/*
 * The obtain function will be called when the buffer is to be filled; it should
 * fill exactly 'len' bytes of 'buf' starting at offset 'offset'. 'priv' is
 * available for the function itself and will not be touched.
 *
 * As a kludge, this function will b called with buf = NULL, offset = 0 and len
 * = 0 if the resource is to be freed.
 */
typedef errorcode_t (*exec_obtain_fn)(void* priv, void* buf, off_t offset, size_t len);

/*
 * An executable handler should use the obtainfn as described above to set up thread t
 * so that it will be available to run, or return an error code. 
 */
typedef errorcode_t (*exec_handler_t)(thread_t* t, void* priv, exec_obtain_fn obtainfn);

/*
 * Define an executable format.
 */
struct EXEC_FORMAT {
	/* Human-readable identifier */
	const char*	ef_identifier;

	/* Function handling the execution */
	exec_handler_t	ef_handler;

	/* Queue fields */
	DQUEUE_FIELDS(struct EXEC_FORMAT);
};
DQUEUE_DEFINE(EXEC_FORMATS, struct EXEC_FORMAT);

#define EXECUTABLE_FORMAT(id, handler) \
	static struct EXEC_FORMAT execfmt_##handler; \
	static errorcode_t register_##handler() { \
		return exec_register_format(&execfmt_##handler); \
	}; \
	static errorcode_t unregister_##handler() { \
		return exec_unregister_format(&execfmt_##handler); \
	}; \
	INIT_FUNCTION(register_##handler, SUBSYSTEM_THREAD, ORDER_MIDDLE); \
	EXIT_FUNCTION(unregister_##handler); \
	static struct EXEC_FORMAT execfmt_##handler = { \
		.ef_identifier = id, \
		.ef_handler = &handler \
	};

errorcode_t exec_launch(thread_t* t, void* priv, exec_obtain_fn obtain);
errorcode_t exec_register_format(struct EXEC_FORMAT* ef);
errorcode_t exec_unregister_format(struct EXEC_FORMAT* ef);

#endif /* __ANANAS_EXEC_H__ */
