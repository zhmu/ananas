#include "lib.h"

register_t syscall1(register_t no, register_t arg1);

void
putchar(int c)
{
	/*
	 * XXX this syscall won't exist forever, but we need something to
	 * interact... let's prefix with 0xfffe to state this for now XXX
	 */
	syscall1(0xfffe0001, c);
}

/* vim:set ts=2 sw=2: */
