#ifndef __INIT_H__
#define __INIT_H__

#include <ananas/list.h>

typedef errorcode_t (*init_func_t)();
typedef errorcode_t (*exit_func_t)();

enum INIT_SUBSYSTEM {
	SUBSYSTEM_MODULE	= 0x0080000,
	/* Everything above here cannot be used by modules */
	SUBSYSTEM_HANDLE	= 0x0100000,
	SUBSYSTEM_KDB		= 0x0110000,
	SUBSYSTEM_DRIVER	= 0x0118000,
	SUBSYSTEM_CONSOLE	= 0x0120000,
	/* After this point, there is a console */
	SUBSYSTEM_PROCESS	= 0x01f0000,
	SUBSYSTEM_THREAD	= 0x0200000,
	SUBSYSTEM_SMP		= 0x0210000,
	SUBSYSTEM_WAITQUEUE	= 0x0220000,
	SUBSYSTEM_TTY		= 0x0230000,
	SUBSYSTEM_BIO		= 0x0240000,
	SUBSYSTEM_DEVICE	= 0x0250000,
	SUBSYSTEM_VFS		= 0x0260000,
	SUBSYSTEM_SCHEDULER	= 0xf000000,
	/* Do not use the last subsystem */
	SUBSYSTEM_LAST		= 0xfffffff
};

enum INIT_ORDER {
	ORDER_FIRST		= 0x0000001,
	ORDER_SECOND		= 0x0000002,
	ORDER_MIDDLE		= 0x4000000,
	ORDER_ANY		= 0x8000000,
	ORDER_LAST		= 0xfffffff
};

struct INIT_FUNC {
	init_func_t		if_func;
	enum INIT_SUBSYSTEM	if_subsystem;
	enum INIT_ORDER		if_order;
};

struct EXIT_FUNC {
	exit_func_t	ef_func;
};

/*
 * Defines an initialization function; these will be placed in a specific ELF
 * section so that any file can add functions as it requires; during
 * initalization, they will be started.
 *
 */
#define INIT_FUNCTION(fn,subsys,order) \
	static const struct INIT_FUNC if_##fn = { \
		.if_func = fn, \
		.if_subsystem = subsys, \
		.if_order = order \
	}; \
	extern "C" void const * const __if_include_##fn __attribute__((section("initfuncs"))) __attribute__((unused)) = &if_##fn

/*
 * Defines an exit function; currently only used for modules.
 */
#define EXIT_FUNCTION(fn) \
	static const struct EXIT_FUNC ef_##fn = { \
		.ef_func = fn \
	}; \
	extern "C" const void * const __ef_include_##fn __attribute__((section("exitfuncs")))  __attribute__((unused)) = &ef_##fn

void mi_startup();

#endif /* __INIT_H__ */
