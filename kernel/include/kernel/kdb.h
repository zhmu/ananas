#ifndef __ANANAS_KDB_H__
#define __ANANAS_KDB_H__

#include <ananas/util/list.h>
#include "kernel/init.h"

class Result;

enum KDB_ARG_TYPE {
	T_INVALID, /* internal use only */
	T_STRING,
	T_NUMBER,
	T_BOOL
};
typedef enum KDB_ARG_TYPE kdb_arg_type_t;

/*
 * An argument as passed to the command's backing function.
 */
struct KDB_ARG {
	kdb_arg_type_t a_type;
	union {
		const char* u_string;
		unsigned long u_value;
	} a_u;
};

/* Callback function for a given command */
typedef void kdb_func_t(int num_args, struct KDB_ARG args[]);

/* KDB command itself */
struct KDBCommand : util::List<KDBCommand>::NodePtr {
	KDBCommand(const char* cmd, const char* args, const char* help, kdb_func_t* func)
	 : cmd_command(cmd), cmd_args(args), cmd_help(help), cmd_func(func)
	{
	}
	const char* cmd_command;
	const char* cmd_args;
	const char* cmd_help;
	kdb_func_t* cmd_func;
};

void kdb_enter(const char* why);
void kdb_panic();
Result kdb_register_command(KDBCommand& cmd);

#define KDB_COMMAND(CMD, ARGS, HELP) \
	static kdb_func_t kdb_func_ ## CMD; \
	static KDBCommand kdb_cmd_ ## CMD(#CMD, (ARGS), (HELP), &kdb_func_##CMD); \
	static Result kdb_add_ ## CMD() { \
		return kdb_register_command(kdb_cmd_ ## CMD); \
	} \
	INIT_FUNCTION(kdb_add_ ## CMD , SUBSYSTEM_KDB, ORDER_MIDDLE); \
	static void kdb_func_ ## CMD(int num_args, struct KDB_ARG arg[])

#endif /* __ANANAS_KDB_H__ */
