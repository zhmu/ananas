#include <unistd.h>
#include <_posix/fdmap.h>

int getdtablesize()
{
	return FDMAP_SIZE;
}
