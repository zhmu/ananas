#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <_posix/error.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

int execlp(const char *path, const char* arg0, ...)
{
	errorcode_t err;
	va_list va;

	/*
	 * Determine the length of the arguments.
	 */
	int args_len = strlen(arg0) + 1 /* arg0 \0 */ + 1 /* terminating \0 */;
	va_start(va, arg0);
	while(1) {
		const char* arg = va_arg(va, const char*);
		if (arg == NULL)
			break;
		args_len += strlen(arg) + 1;
	}

	/* Construct the argument list */
	char* args = malloc(args_len);
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
	args[pos] = '\0'; /* double \0 termination */

	/* Now, open the executable */
	void* handle;
	struct OPEN_OPTIONS openopts;
	memset(&openopts, 0, sizeof(openopts));
	openopts.op_size = sizeof(openopts);
	openopts.op_path = path;
	openopts.op_mode = OPEN_MODE_READ;
	err = sys_open(&openopts, &handle);
	if (err != ANANAS_ERROR_NONE) {
		free(args);
		goto fail;
	}

	/* Attempt to invoke the executable just opened */
	void* child;
	struct SUMMON_OPTIONS summonopts;
	memset(&summonopts, 0, sizeof(summonopts));
	summonopts.su_size = sizeof(summonopts);
	summonopts.su_flags = SUMMON_FLAG_RUNNING;
	summonopts.su_args_len = args_len;
	summonopts.su_args = args;
	err = sys_summon(handle, &summonopts, &child);
	free(args);
	sys_destroy(handle);
	if (err == ANANAS_ERROR_NONE) {
		/* thread worked; wait for it to die */
		handle_event_t event = HANDLE_EVENT_ANY;
		handle_event_result_t result;
		err = sys_wait(child, &event, &result);
		sys_destroy(child);
		if (err != ANANAS_ERROR_NONE)
			goto fail;

		/*
		 * Now, we have to commit suicide using the exit code of the
	 	 * previous thread...
		 */
		sys_exit((int)result);
	}

fail:
	/* Summoning failed - report the code and leave */
	_posix_map_error(err);
	return -1;
}

/* vim:set ts=2 sw=2: */
