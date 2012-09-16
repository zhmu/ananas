#include <unistd.h>
#include <ananas/threadinfo.h>
#include <_posix/handlemap.h>
#include <string.h>

static struct HANDLEMAP_ENTRY handle_map[HANDLEMAP_SIZE];
extern struct HANDLEMAP_OPS* handlemap_ops[];

void
handlemap_reinit(struct THREADINFO* ti)
{
	/* Hook up POSIX standard file handles */
	handlemap_set_entry(STDIN_FILENO,  HANDLEMAP_TYPE_FD, ti->ti_handle_stdin);
	handlemap_set_entry(STDOUT_FILENO, HANDLEMAP_TYPE_FD, ti->ti_handle_stdout);
	handlemap_set_entry(STDERR_FILENO, HANDLEMAP_TYPE_FD, ti->ti_handle_stderr);
}

void
handlemap_init(struct THREADINFO* ti)
{
	memset(handle_map, 0, HANDLEMAP_SIZE * sizeof(struct HANDLEMAP_ENTRY));
	handlemap_reinit(ti);
}

int
handlemap_alloc_entry(int type, void* handle)
{
	for (unsigned int idx = 0; idx < HANDLEMAP_SIZE; idx++) {
		if (handle_map[idx].hm_type != HANDLEMAP_TYPE_UNUSED)
			continue;
		handlemap_set_entry(idx, type, handle);
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
handlemap_set_entry(int idx, int type, void* handle)
{
	handle_map[idx].hm_type = type;
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
	if (handle_map[idx].hm_type == HANDLEMAP_TYPE_UNUSED)
		return NULL;
	return handle_map[idx].hm_handle;
}

int
handlemap_get_type(int idx)
{
	if (idx < 0 || idx >= HANDLEMAP_SIZE)
		return HANDLEMAP_TYPE_UNUSED;
	return handle_map[idx].hm_type;
}

struct HANDLEMAP_OPS*
handlemap_get_ops(int idx)
{
	if (idx < 0 || idx >= HANDLEMAP_SIZE)
		return NULL;
	if (handle_map[idx].hm_type == HANDLEMAP_TYPE_UNUSED)
		return NULL;
	return handlemap_ops[handle_map[idx].hm_type];
}

/* vim:set ts=2 sw=2: */
