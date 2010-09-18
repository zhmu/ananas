#include <ananas/threadinfo.h>
#include <_posix/fdmap.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct THREADINFO* libc_threadinfo;
int    libc_argc = 0;
char** libc_argv = NULL;
char** environ = { NULL };

void
libc_init(struct THREADINFO* ti)
{
	libc_threadinfo = ti;
	fdmap_init(ti);

	/* Count the number of arguments */
	int num_args = 0;
	const char* cur_ptr = ti->ti_args;
	while (1) {
		const char* ptr = strchr(cur_ptr, '\0');
		if (ptr == cur_ptr)
			break;
		cur_ptr = ptr + 1;
		num_args++;
	}

	/* Copy the argument pointers in place */
	libc_argv = malloc(sizeof(const char*) * (num_args + 1 /* terminating NULL */));
	libc_argc = 0;
	cur_ptr = ti->ti_args;
	while (1) {
		libc_argv[libc_argc] = cur_ptr;
		const char* ptr = strchr(cur_ptr, '\0');
		if (ptr == cur_ptr)
			break;
		cur_ptr = ptr + 1;
		libc_argc++;
	}
	libc_argv[libc_argc] = NULL;
	assert(libc_argc == num_args);
}

/* vim:set ts=2 sw=2: */
