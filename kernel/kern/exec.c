#include <ananas/exec.h>
#include <ananas/lib.h>
#include <ananas/init.h>
#include <ananas/trace.h>

TRACE_SETUP;

static struct EXEC_FORMATS exec_formats; /* XXX do we need to lock this? */

static errorcode_t
exec_init()
{
	DQUEUE_INIT(&exec_formats);
	return ANANAS_ERROR_OK;
}

errorcode_t
exec_launch(thread_t* t, void* priv, exec_obtain_fn obtain)
{
	errorcode_t err = ANANAS_ERROR(BAD_EXEC);
	if (!DQUEUE_EMPTY(&exec_formats))
		DQUEUE_FOREACH(&exec_formats, ef, struct EXEC_FORMAT) {
			err = ef->ef_handler(t, priv, obtain);
			if (err == ANANAS_ERROR_OK)
				break;
		}

	return err;
}

INIT_FUNCTION(exec_init, SUBSYSTEM_THREAD, ORDER_FIRST);

errorcode_t
exec_register_format(struct EXEC_FORMAT* ef)
{
	DQUEUE_ADD_TAIL(&exec_formats, ef);
	return ANANAS_ERROR_OK;
}

errorcode_t
exec_unregister_format(struct EXEC_FORMAT* ef)
{
	DQUEUE_REMOVE(&exec_formats, ef);
	return ANANAS_ERROR_OK;
}

/* vim:set ts=2 sw=2: */
