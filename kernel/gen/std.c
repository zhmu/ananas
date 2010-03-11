#include <sys/lib.h>
#include <sys/pcpu.h>
#include <sys/thread.h>

void
sys_exit()
{
	panic("sys_exit");
}

void*
sys_map(size_t len)
{
	thread_t thread = PCPU_GET(curthread);
	struct THREAD_MAPPING* tm = thread_map(thread, NULL, len, THREAD_MAP_ALLOC);
	return (void*)tm->start;
}

int
sys_unmap(void* ptr, size_t len)
{
	thread_t thread = PCPU_GET(curthread);
	return thread_unmap(thread, ptr, len);
}

/* vim:set ts=2 sw=2: */
