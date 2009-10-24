#include "thread.h"
#include "amd64/thread.h"

int
md_thread_init(thread_t thread)
{
	/* TODO */
	return 1;
}

size_t
md_thread_get_privdata_length()
{
	return sizeof(struct MD_THREAD);
}

void
md_thread_destroy(thread_t thread)
{
	/* TODO */
}

void
md_thread_switch(thread_t new, thread_t old)
{
	/* TODO */
}

/* vim:set ts=2 sw=2: */
