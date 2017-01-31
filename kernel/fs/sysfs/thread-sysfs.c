#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/thread.h>
#include <ananas/fs/sysfs.h>
#include <ananas/lib.h>

static struct SYSFS_ENTRY* thread_sysfs_root = NULL;

static errorcode_t
sysfs_init_thread(thread_t* thread, thread_t* parent)
{
	char name[64 /* XXX */ ];
	name[snprintf(name, sizeof(name), "%p", thread)] = '\0';

	struct SYSFS_ENTRY* root = sysfs_mkdir(thread_sysfs_root, name, 0555);

	(void)root;

	return ANANAS_ERROR_NONE;
}

static errorcode_t
sysfs_exit_thread(thread_t* thread, thread_t* parent)
{
	return ANANAS_ERROR_NONE;
}

static errorcode_t
sysfs_thread_init()
{
	thread_sysfs_root = sysfs_mkdir(NULL, "thread", 0555);
	return ANANAS_ERROR_NONE;
}

INIT_FUNCTION(sysfs_thread_init, SUBSYSTEM_THREAD, ORDER_FIRST);
REGISTER_THREAD_INIT_FUNC(sysfs_init_thread);
REGISTER_THREAD_EXIT_FUNC(sysfs_exit_thread);

/* vim:set ts=2 sw=2: */
