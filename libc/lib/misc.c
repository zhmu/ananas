#include <types.h>
#include <stdio.h>

void
abort()
{
	printf("abort\n");
	/* XXX horrible */
	*(unsigned int*)NULL = 0;
}

void
__assert(const char* func, const char* file, int line, const char* msg)
{
	printf("assertion failed: (%s) in %s:%d\n", msg, file, line);
	abort();
}
