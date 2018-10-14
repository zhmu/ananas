#ifndef __ANANAS_EXEC_H__
#define __ANANAS_EXEC_H__

#include <ananas/types.h>
#include <ananas/util/list.h>
#include "kernel/init.h"

struct DEntry;
class Result;
class Thread;
class VMSpace;

struct IExecutor
{
	virtual Result Load(VMSpace& vs, DEntry& dentry, void*& auxargs) = 0;
	virtual Result PrepareForExecute(VMSpace& vs, Thread& t, void* auxargs, const char* argv[], const char* envp[]) = 0;
};

/*
 * Define an executable format.
 */
struct ExecFormat : util::List<ExecFormat>::NodePtr {
	ExecFormat(const char* id, IExecutor& executor)
	 : ef_identifier(id), ef_executor(executor)
	{
	}

	/* Human-readable identifier */
	const char*	ef_identifier;

	/* Function handling the execution */
	IExecutor&	ef_executor;
};

#define EXECUTABLE_FORMAT(id, handler) \
	static ExecFormat execfmt_##handler(id, handler); \
	static Result register_##handler() { \
		return exec_register_format(execfmt_##handler); \
	}; \
	static Result unregister_##handler() { \
		return exec_unregister_format(execfmt_##handler); \
	}; \
	INIT_FUNCTION(register_##handler, SUBSYSTEM_THREAD, ORDER_MIDDLE); \
	EXIT_FUNCTION(unregister_##handler);

Result exec_load(VMSpace& vs, DEntry& dentry, IExecutor*& executor, void*& auxargs);
Result exec_register_format(ExecFormat& ef);
Result exec_unregister_format(ExecFormat& ef);

#endif /* __ANANAS_EXEC_H__ */
