#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/handle.h>
#include <sys/stat.h>
#include <syscalls.h>

int execve(const char *path, char *const argv[], char *const envp[])
{
	/* Construct the argument length */
	int len = 1; /* terminating \0 */
	for (int i = 0; argv[i] != NULL; i++) {
		len += strlen(argv[i]) + 1;
	}

	/* Construct the argument list */
	char* args = malloc(len);
	if (args == NULL) {
		errno = ENOMEM;
		return -1;
	}
	int pos = 0;
	for (int i = 0; argv[i] != NULL; i++) {
		strcpy((args + pos), argv[i]);
		pos += strlen(argv[i]) + 1;
	}
	args[pos] = '\0';

	void* handle = sys_open(path, O_RDONLY);
	if (handle == NULL) {
		free(args);
		return -1;
	}

	void* newhandle = sys_summon(handle, 0, args);
	free(args);
	sys_close(handle);
	if (newhandle != NULL) {
		/* thread worked; wait for it to die */
		handle_event_result_t e = sys_wait(newhandle, NULL);
		sys_close(newhandle);

		/*
		 * Now, we have to commit suicide using the exit code of the
	 	 * previous thread...
		 */
		sys_exit((int)e);
	}

	return NULL;
}

/* vim:set ts=2 sw=2: */
