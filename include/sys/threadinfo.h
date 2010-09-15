#include <sys/types.h>

#ifndef __SYS_THREADINFO_H__
#define __SYS_THREADINFO_H__

#define THREADINFO_ARGS_LENGTH	256

struct THREADINFO {
	int	ti_size;				/* structure length */
	void*	ti_handle;				/* thread handle */
	void*	ti_handle_stdin;			/* stdin handle */
	void*	ti_handle_stdout;			/* stdout handle */
	void*	ti_handle_stderr;			/* stderr handle */
	char	ti_args[THREADINFO_ARGS_LENGTH];	/* commandline arguments */
};

#ifndef KERNEL
extern struct THREADINFO* libc_threadinfo;
#endif

#endif /* __SYS_THREADINFO_H__ */
