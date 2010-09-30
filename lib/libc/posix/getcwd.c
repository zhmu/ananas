#include <unistd.h>
#include <string.h>
#include <errno.h>

extern char _posix_cwd[];

char* getcwd( char* buf, size_t size)
{
	if (size == 0) {
		errno = EINVAL;
		return NULL;
	}
	if (strlen(_posix_cwd) >= size) {
		errno = ERANGE;
		return NULL;
	}
	strcpy(buf, _posix_cwd);
	return buf;
}
