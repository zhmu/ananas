#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <_posix/error.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

int execve(const char *path, char *const argv[], char *const envp[])
{
	errorcode_t err;

	/* Construct the argument length */
	int args_len = 1; /* terminating \0 */
	for (int i = 0; argv[i] != NULL; i++) {
		args_len += strlen(argv[i]) + 1;
	}

	/* Construct the argument list */
	char* args = malloc(args_len);
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
	/* XXX env */
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
