#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ananas/handle.h>
#include <ananas/stat.h>
#include <syscalls.h>

int execlp(const char *path, const char* arg0, ...)
{
	va_list va;

	/*
	 * Determine the length of the arguments.
	 */
	int len = strlen(arg0) + 1 /* arg0 \0 */ + 1 /* terminating \0 */;
	va_start(va, arg0);
	while(1) {
		const char* arg = va_arg(va, const char*);
		if (arg == NULL)
			break;
		len += strlen(arg) + 1;
	}

	/* Construct the argument list */
	char* args = malloc(len);
	if (args == NULL) {
		errno = ENOMEM;
		return -1;
	}
	int pos = 0;
	va_start(va, arg0);
	while(1) {
		const char* arg = va_arg(va, const char*);
		if (arg == NULL)
			break;
		strcpy((args + pos), arg);
		pos += strlen(arg) + 1;
	}
	args[pos] = '\0';

	void* handle = sys_open(path, O_RDONLY);
	if (handle == NULL) {
		free(args);
		errno = ENOENT;
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

	return -1;
}

/* vim:set ts=2 sw=2: */
