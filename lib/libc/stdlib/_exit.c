#include <unistd.h>
#include <stdlib.h>

void _exit(int status)
{
	return _Exit(status);
}
