#include <sys/lib.h> 
#include <loader/diskio.h>
#include <loader/platform.h>
#include <ofw.h>

void*
platform_get_memory(uint32_t length)
{
	return ofw_heap_alloc(length);
}

void
platform_putch(uint8_t ch)
{
	ofw_putch(ch);
}

int
platform_getch()
{
	return 0;
}

int
platform_init_disks()
{
	return 0;
}

int
platform_read_disk(int disk, uint32_t lba, void* buffer, int num_bytes)
{
	return 0;
}

void
platform_reboot()
{
}

void
platform_cleanup()
{
}

int
platform_init_netboot()
{
	return 0;
}

uint64_t
platform_init_memory_map()
{
	return ofw_get_memory_size();
}

void
platform_init(int r3, int r4, unsigned int r5)
{
	ofw_init();

	main();
}

/* vim:set ts=2 sw=2: */
