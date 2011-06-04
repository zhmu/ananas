#include <ananas/threadinfo.h>
#include <_posix/init.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
putenv(char* string)
{
	/*
	 * First of all, locate the end of the environment structure; we'll need to
	 * append the string to there so we can immediately check whether it fits.
	 */
	int i = 0;
	while(i < THREADINFO_ENV_LENGTH - 2) {
		if (libc_threadinfo->ti_env[i    ] == '\0' &&
		    libc_threadinfo->ti_env[i + 1] == '\0')
			break;
		i++;
	}
	if (i + strlen(string) >= THREADINFO_ENV_LENGTH) {
		/* No space left */
		errno = ENOMEM;
		return -1;
	}

	const char* ptr = strchr(string, '=');
	if (ptr != NULL) {
		/*
		 * See if we have the same variable key present; if so, we'll need to
		 * remove it. Note that this uses the implementation specifics of
		 * libc_reinit_environ() - namely that it'll store a pointer to something
		 * that is in the ti_env field; we'll use this to remove the key if
		 * necessary.
		 */
		char** env = environ;
		while (*env != NULL) {
			if (strncmp(*env, string, ptr - string) == 0)
				break;
			env++;
		}
		if (*env != NULL) {
			/* Got a match; need to remove it */
			i -= strlen(*env) + 1;
			memcpy(*env, *env + strlen(*env) + 1, THREADINFO_ENV_LENGTH - (*env - libc_threadinfo->ti_env));
		}
	} else {
		/*
		 * According to POSIX, we may not refuse even though the passed
		 * string is bogus; so we'll just add it to the environment.
		 */
	}

	/* Add the string and double-\0-terminate it */
	strcpy(&libc_threadinfo->ti_env[i + 1], string);
	libc_threadinfo->ti_env[i + strlen(string) + 2] = '\0';

	/* Reconstruct the environ array */
	libc_reinit_environ();
	return 0;
}

/* vim:set ts=2 sw=2: */
