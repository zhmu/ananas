/*
 * Init functionality; largely inspired by NetBSD's lib/csu/common/crt0-common.c.
 */
#include <stdlib.h>

extern int main(int argc, char** argv, char** envp);
extern void exit(int);
char** environ;

#define __hidden __attribute__((__visibility__("hidden")))

extern void _fini(void) __hidden;
extern void _init(void) __hidden;

extern void* _DYNAMIC;
#pragma weak _DYNAMIC

void
__start(unsigned long* stk, void (*cleanup)() /* provided by rtld-elf */)
{
	int argc = *stk++;
	char** argv = (char**)stk;
	stk += argc + 1 /* terminating null */ ;
	environ = (char**)stk++;
	void* auxv = stk;
	if (&_DYNAMIC != NULL)
		atexit(cleanup);
	atexit(_fini);
	_init();

	exit(main(argc, argv, environ));
}
