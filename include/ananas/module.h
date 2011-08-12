#ifndef __ANANAS_MODULE_H__
#define __ANANAS_MODULE_H__

#include <ananas/dqueue.h>

/*
 * Module-specific initalization function.
 */
#define MOD_INIT_FUNC_NAME "mod_init"
typedef errorcode_t (*mod_init_func_t)();

/*
 * Module-specific deinitialization function.
 */
#define MOD_EXIT_FUNC_NAME "mod_exit"
typedef errorcode_t (*mod_exit_func_t)();

struct KERNEL_MODULE {
	/* Init/exit functions */
	mod_init_func_t	kmod_init_func;
	mod_exit_func_t	kmod_exit_func;

	/* Pointers to module contents */
	void* kmod_strptr;			/* Strings */
	void* kmod_symptr;			/* Symbols */
	size_t kmod_sym_size;			/* Size of symbols (bytes) */
	void* kmod_rwdataptr;			/* RW data */
	void* kmod_rodataptr;			/* RO data */
	void* kmod_codeptr;			/* Code */

	DQUEUE_FIELDS(struct KERNEL_MODULE);
};

DQUEUE_DEFINE(KERNEL_MODULE_QUEUE, struct KERNEL_MODULE);

#endif /* __ANANAS_MODULE_H__ */
