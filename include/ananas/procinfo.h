#include <ananas/types.h>
#include <ananas/_types/errorcode.h>

#ifndef __SYS_PROCINFO_H__
#define __SYS_PROCINFO_H__

#define PROCINFO_ARGS_LENGTH	1024
#define PROCINFO_ENV_LENGTH	1024

struct PROCINFO {
	int		pi_size;				/* structure length */
	pid_t		pi_pid;					/* process ID */
	char		pi_args[PROCINFO_ARGS_LENGTH];	/* commandline arguments */
	char		pi_env[PROCINFO_ENV_LENGTH];		/* environment */
};

#ifndef KERNEL
extern struct PROCINFO* ananas_procinfo;
#endif

#endif /* __SYS_PROCINFO_H__ */
