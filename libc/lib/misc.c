#include <types.h>
#include <stdio.h>

void
abort()
{
	printf("abort\n");
	/* XXX horrible */
	*(unsigned int*)NULL = 0;
}
