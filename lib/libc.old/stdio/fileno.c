#include <stdio.h>
#include <stdlib.h>

int fileno( FILE * stream )
{
	return stream->handle;
}
