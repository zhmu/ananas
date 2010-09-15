#include <sys/stat.h>

int mknod(const char* path, mode_t mode, dev_t dev)
{
	/* TODO - likely will never be supported */
	return -1;
}
