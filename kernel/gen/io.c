#include "types.h"
#include "lib.h"

ssize_t
sys_read(int fd, void* buf, size_t count)
{
	panic("sys_read");
	return -1;
}

ssize_t
sys_write(int fd, const void* buf, size_t count)
{
	kprintf("sys_write: fd=%u, buf=%p=, count=%u\n", fd, buf, count);
	return -1;
}

/* vim:set ts=2 sw=2: */
