#include <ananas/thread.h>

errorcode_t 
md_thread_init(thread_t* t)
{
	return -1;
}

errorcode_t
md_kthread_init(thread_t* t, kthread_func_t kfunc, void* arg)
{
	return -1;
}

void
md_thread_free(thread_t* t)
{
}

void
md_thread_switch(thread_t* new, thread_t* old)
{
}

void*
md_map_thread_memory(thread_t* thread, void* ptr, size_t length, int write)
{
	return NULL;
}

void*
md_thread_map(thread_t* thread, void* to, void* from, size_t length, int flags)
{
	return NULL;
}

addr_t
md_thread_is_mapped(thread_t* thread, addr_t virt, int flags)
{
	return 0;
}

void
md_thread_set_argument(thread_t* thread, addr_t arg)
{
}

void
md_thread_clone(thread_t* t, thread_t* parent, register_t retval)
{
}

void
md_reschedule()
{
}

/* vim:set ts=2 sw=2: */
