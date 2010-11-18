#include <ananas/types.h>
#include <ananas/_types/errorcode.h>

#ifndef __SYS_THREADINFO_H__
#define __SYS_THREADINFO_H__

#define THREADINFO_ARGS_LENGTH	256
#define THREADINFO_ENV_LENGTH	1024

struct THREADINFO {
	int		ti_size;				/* structure length */
	handle_t	ti_handle;				/* thread handle */
	handle_t	ti_handle_stdin;			/* stdin handle */
	handle_t	ti_handle_stdout;			/* stdout handle */
	handle_t	ti_handle_stderr;			/* stderr handle */
	char		ti_args[THREADINFO_ARGS_LENGTH];	/* commandline arguments */
	char		ti_env[THREADINFO_ENV_LENGTH];		/* environment */
};

#ifndef KERNEL
extern struct THREADINFO* libc_threadinfo;
#endif

#endif /* __SYS_THREADINFO_H__ */
