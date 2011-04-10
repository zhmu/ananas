#include <unistd.h>
#include <ananas/threadinfo.h>
#include <_posix/handlemap.h>
#include <string.h>

static struct HANDLEMAP_ENTRY handle_map[HANDLEMAP_SIZE];

void
handlemap_init(struct THREADINFO* ti)
{
	memset(handle_map, 0, HANDLEMAP_SIZE * sizeof(struct HANDLEMAP_ENTRY));

	/* Hook up POSIX standard file handles */
	handle_map[STDIN_FILENO ].hm_type   = HANDLEMAP_TYPE_FD;
	handle_map[STDIN_FILENO ].hm_handle = ti->ti_handle_stdin;
	handle_map[STDOUT_FILENO].hm_type   = HANDLEMAP_TYPE_FD;
	handle_map[STDOUT_FILENO].hm_handle = ti->ti_handle_stdout;
	handle_map[STDERR_FILENO].hm_type   = HANDLEMAP_TYPE_FD;
	handle_map[STDERR_FILENO].hm_handle = ti->ti_handle_stderr;
}

int
handlemap_alloc_entry(int type, void* handle)
{
	for (unsigned int idx = 0; idx < HANDLEMAP_SIZE; idx++) {
		if (handle_map[idx].hm_type != HANDLEMAP_TYPE_UNUSED)
			continue;
		handle_map[idx].hm_type = type;
		handle_map[idx].hm_handle = handle;
		return idx;
	}
	return -1;
}

void
handlemap_set_handle(int idx, void* handle)
{
	handle_map[idx].hm_handle = handle;
}

void
handlemap_free_entry(int idx)
{
	handle_map[idx].hm_type = HANDLEMAP_TYPE_UNUSED;
}

void*
handlemap_deref(int idx, int type)
{
	if (idx < 0 || idx >= HANDLEMAP_SIZE)
		return NULL;
	if (type != HANDLEMAP_TYPE_ANY && handle_map[idx].hm_type != type)
		return NULL;
	return handle_map[idx].hm_handle;
}

/* vim:set ts=2 sw=2: */
