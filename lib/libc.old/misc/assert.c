#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void
__assert(const char* func, const char* file, int line, const char* expr)
{
	fprintf(stderr, "assertion failed: %s in %s:%u, %s\n", expr, file, line, func);
	abort();
}
