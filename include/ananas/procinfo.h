#include <ananas/types.h>
#include <ananas/_types/errorcode.h>

#ifndef __SYS_PROCINFO_H__
#define __SYS_PROCINFO_H__

#define PROCINFO_ARGS_LENGTH	1024
#define PROCINFO_ENV_LENGTH	1024

struct PROCINFO {
	int		pi_size;				/* structure length */
	handleindex_t	pi_handle_main;				/* main thread handle */
	handleindex_t	pi_handle_stdin;			/* stdin handle */
	handleindex_t	pi_handle_stdout;			/* stdout handle */
	handleindex_t	pi_handle_stderr;			/* stderr handle */
	char		pi_args[PROCINFO_ARGS_LENGTH];	/* commandline arguments */
	char		pi_env[PROCINFO_ENV_LENGTH];		/* environment */
};

#ifndef KERNEL
extern struct PROCINFO* libc_procinfo;
#endif

#endif /* __SYS_PROCINFO_H__ */
