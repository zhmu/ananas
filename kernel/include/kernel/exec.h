#ifndef __ANANAS_EXEC_H__
#define __ANANAS_EXEC_H__

#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/util/list.h>
#include "kernel/init.h"

struct DEntry;
class VMSpace;

typedef errorcode_t (*exec_handler_t)(VMSpace& vs, DEntry& dentry, addr_t* exec_addr, register_t* exec_arg);

/*
 * Define an executable format.
 */
struct ExecFormat : util::List<ExecFormat>::NodePtr {
	ExecFormat(const char* id, exec_handler_t handler)
	 : ef_identifier(id), ef_handler(handler)
	{
	}

	/* Human-readable identifier */
	const char*	ef_identifier;

	/* Function handling the execution */
	exec_handler_t	ef_handler;
};

#define EXECUTABLE_FORMAT(id, handler) \
	static ExecFormat execfmt_##handler(id, &handler); \
	static errorcode_t register_##handler() { \
		return exec_register_format(execfmt_##handler); \
	}; \
	static errorcode_t unregister_##handler() { \
		return exec_unregister_format(execfmt_##handler); \
	}; \
	INIT_FUNCTION(register_##handler, SUBSYSTEM_THREAD, ORDER_MIDDLE); \
	EXIT_FUNCTION(unregister_##handler);

errorcode_t exec_load(VMSpace& vs, DEntry& dentry, addr_t* exec_addr, register_t* exec_arg);
errorcode_t exec_register_format(ExecFormat& ef);
errorcode_t exec_unregister_format(ExecFormat& ef);

#endif /* __ANANAS_EXEC_H__ */
