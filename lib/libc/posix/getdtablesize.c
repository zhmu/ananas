#include <unistd.h>
#include <_posix/handlemap.h>

int getdtablesize()
{
	return HANDLEMAP_SIZE;
}
