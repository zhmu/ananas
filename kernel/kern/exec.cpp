#include <ananas/errno.h>
#include "kernel/exec.h"
#include "kernel/lib.h"
#include "kernel/init.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/vfs/dentry.h"

TRACE_SETUP;

static util::List<ExecFormat> exec_formats; /* XXX do we need to lock this? */

Result
exec_load(VMSpace& vs, DEntry& dentry, addr_t* exec_addr, register_t* exec_arg)
{
	// Start by taking an extra ref to the dentry; this is the ref which we'll hand over
	// to the handler, if all goes well
	dentry_ref(dentry);

	for(auto& ef: exec_formats) {
		/* See if we can execute this... */
		if (Result result = ef.ef_handler(vs, dentry, exec_addr, exec_arg); result.IsFailure()) {
			/* Execute failed; try the next one */
			continue;
		}

		return Result::Success();
	}

	/* Nothing worked... return our ref */
	dentry_deref(dentry);
	return RESULT_MAKE_FAILURE(ENOEXEC);
}

Result
exec_register_format(ExecFormat& ef)
{
	exec_formats.push_back(ef);
	return Result::Success();
}

Result
exec_unregister_format(ExecFormat& ef)
{
	exec_formats.remove(ef);
	return Result::Success();
}

/* vim:set ts=2 sw=2: */
