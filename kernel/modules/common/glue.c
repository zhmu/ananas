#include <ananas/types.h>
#include <ananas/module.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/init.h>

extern void* __initfuncs_begin;
extern void* __initfuncs_end;
extern void* __exitfuncs_begin;
extern void* __exitfuncs_end;

errorcode_t
mod_init()
{
	/* Walk through the entire init function chain and execute everything */
	for (struct INIT_FUNC** ifn = (struct INIT_FUNC**)&__initfuncs_begin; ifn < (struct INIT_FUNC**)&__initfuncs_end; ifn++) {
		errorcode_t err = (*ifn)->if_func();
		if (err != ANANAS_ERROR_OK)
			return err;
	}
	return ANANAS_ERROR_OK;
}

errorcode_t
mod_exit()
{
	/* Walk through the entire exit function chain and execute everything */
	for (struct EXIT_FUNC** efn = (struct EXIT_FUNC**)&__exitfuncs_begin; efn < (struct EXIT_FUNC**)&__exitfuncs_end; efn++) {
		errorcode_t err = (*efn)->ef_func();
		if (err != ANANAS_ERROR_OK)
			return err;
	}
	return ANANAS_ERROR_OK;
}

/* vim:set ts=2 sw=2: */
