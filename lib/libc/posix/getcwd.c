#include <unistd.h>

extern int sys_getcwd(char* buf, size_t size);

char* getcwd( char* buf, size_t size)
{
	if (sys_getcwd(buf, size) != 0)
		return NULL;
	return buf;
}
