#ifndef __ANANAS_KDB_H__
#define __ANANAS_KDB_H__

#include <ananas/list.h>
#include <ananas/types.h> /* for errorcode_t */

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
struct KDB_COMMAND {
	const char* cmd_command;
	const char* cmd_args;
	const char* cmd_help;
	kdb_func_t* cmd_func;

	LIST_FIELDS(struct KDB_COMMAND);
};

void kdb_enter(const char* why);
void kdb_panic();
errorcode_t kdb_register_command(struct KDB_COMMAND* cmd);

#define KDB_COMMAND(CMD, ARGS, HELP) \
	static kdb_func_t kdb_func_ ## CMD; \
	static struct KDB_COMMAND kdb_cmd_ ## CMD = { \
		.cmd_command = #CMD, \
		.cmd_args = (ARGS), \
		.cmd_help = (HELP), \
		.cmd_func = &kdb_func_##CMD \
	}; \
	static errorcode_t kdb_add_ ## CMD() { \
		return kdb_register_command(&kdb_cmd_ ## CMD); \
	} \
	INIT_FUNCTION(kdb_add_ ## CMD , SUBSYSTEM_KDB, ORDER_MIDDLE); \
	static void kdb_func_ ## CMD(int num_args, struct KDB_ARG arg[])

#endif /* __ANANAS_KDB_H__ */
