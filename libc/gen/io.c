#include "lib.h"

void
putchar(int c)
{
	write(1 /* STDOUT_FILENO */, &c, 1);
}

/* vim:set ts=2 sw=2: */
