#include <machine/param.h>
#include <machine/thread.h>
#include <machine/vm.h>
#include <sys/thread.h>
#include <sys/lib.h>
#include <sys/mm.h>
#include <sys/pcpu.h>
#include <sys/vm.h>

extern struct TSS kernel_tss;
void md_idle_thread();

int
md_thread_init(thread_t t)
{
}

void
md_thread_destroy(thread_t t)
{
}

void
md_thread_switch(thread_t new, thread_t old)
{
}

void*
md_map_thread_memory(thread_t thread, void* ptr, size_t length, int write)
{
	return NULL;
}

void*
md_thread_map(thread_t thread, void* to, void* from, size_t length, int flags)
{
	return NULL;
}

int
md_thread_unmap(thread_t thread, void* addr, size_t length)
{
	return 0;
}

void
md_thread_setidle(thread_t thread)
{
}

/* vim:set ts=2 sw=2: */
