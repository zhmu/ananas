#include <unistd.h>
#include <_posix/fdmap.h>

extern void* sys_clone(void* handle);

int
dup2(int fildes, int fildes2)
{
	void* handle = fdmap_deref(fildes);
	if (handle  == NULL)
		return -1;
	if (fildes2 < 0 || fildes2 >= FDMAP_SIZE)
		return -1;

	if (fdmap_deref(fildes2) != NULL)
		close(fildes2);

	void* newhandle = sys_clone(handle);
	if (newhandle == NULL)
		return -1;

	fdmap_set_handle(fildes2, newhandle);
	return 0;
}

/* vim:set ts=2 sw=2: */
