#ifndef __INIT_H__
#define __INIT_H__

typedef errorcode_t (*init_func_t)();
typedef errorcode_t (*exit_func_t)();

struct INIT_FUNC {
	init_func_t	if_func;
};

struct EXIT_FUNC {
	exit_func_t	ef_func;
};

void mi_startup();

/*
 * Defines an initialization function; these will be placed in a specific ELF
 * section so that any file can add functions as it requires; during
 * initalization, they will be started.
 *
 */
#define INIT_FUNCTION(fn) \
	static const struct INIT_FUNC if_##fn = { \
		.if_func = fn \
	}; \
	static void const * const __if_include_##fn __attribute__((section("initfuncs"))) = &if_##fn

/*
 * Defines an exit function; currently only used for modules.
 */
#define EXIT_FUNCTION(fn) \
	static const struct EXIT_FUNC ef_##fn = { \
		.ef_func = fn \
	}; \
	static const void * const __ef_include_##fn __attribute__((section("exitfuncs"))) = &ef_##fn

#endif /* __INIT_H__ */
