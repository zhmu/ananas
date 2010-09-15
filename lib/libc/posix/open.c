#include <unistd.h>
#include <_posix/fdmap.h>

extern void* sys_open(const char* filename, int mode);
extern void  sys_close(void* handle);

int open( const char* filename, int mode, ... )
{
        void* handle = sys_open( filename, mode );
	if (handle == NULL)
		return -1;

	int fd = fdmap_alloc_fd(handle);
	if (fd < 0) {
		sys_close(handle);
		return -1;
	}
	return fd;
}
