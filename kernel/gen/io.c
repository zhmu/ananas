#include "types.h"
#include "device.h"
#include "lib.h"
#include "pcpu.h"

extern device_t input_dev;
extern device_t output_dev;

ssize_t
sys_read(int fd, void* buf, size_t count)
{
	void* x = md_map_thread_memory(PCPU_GET(curthread), buf, count, 1);
	if (x == NULL)
		return -1;
	if (fd != 1)
		return 0;

	return input_dev->driver->drv_read(input_dev, x, count); 
}

ssize_t
sys_write(int fd, const void* buf, size_t count)
{
	void* x = md_map_thread_memory(PCPU_GET(curthread), buf, count, 0);
	if (x == NULL)
		return -1;
	if (fd != 1)
		return 0;

	return output_dev->driver->drv_write(output_dev, x, count); 
}

/* vim:set ts=2 sw=2: */
