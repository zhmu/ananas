#include <stdio.h>

int fpurge( FILE * stream )
{
	stream->bufidx = 0;
	return 0;
}
