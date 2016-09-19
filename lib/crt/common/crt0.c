/*
 * Init functionality; largely inspired by NetBSD's lib/csu/common/crt0-common.c.
 */
#include <stdlib.h>

extern int main(int argc, char** argv, char** envp);
extern void exit(int);
extern void libc_init(void* threadinfo);
extern char** environ;

extern int libc_argc;
extern char** libc_argv;

#define __hidden __attribute__((__visibility__("hidden")))

extern void _fini(void) __hidden;
extern void _init(void) __hidden;

void
_start(void* threadinfo)
{
	libc_init(threadinfo);
	atexit(_fini);
	_init();

	exit(main(libc_argc, libc_argv, environ));
}
