#include "lib.h"
#include "pcpu.h"
#include "thread.h"

void
sys_exit()
{
	panic("sys_exit");
}

void*
sys_map(size_t len)
{
	thread_t thread = PCPU_GET(curthread);
	return thread_map(thread, len);
}

int
sys_unmap(void* ptr, size_t len)
{
	thread_t thread = PCPU_GET(curthread);
	return thread_unmap(thread, ptr, len);
}

/* vim:set ts=2 sw=2: */
