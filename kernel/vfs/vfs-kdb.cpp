#include <ananas/types.h>
#include <ananas/vfs/core.h>
#include <ananas/vfs/dentry.h>
#include <ananas/vfs/mount.h>
#include <ananas/vfs/icache.h>
#include <ananas/error.h>
#include <ananas/kdb.h>
#include <ananas/thread.h>
#include <ananas/lib.h>

/*
 * This function is more of a sanity check; it does not lock anything to
 * prevent deadlocks.
 */
KDB_COMMAND(inodes, NULL, "Inode status")
{
}

/* vim:set ts=2 sw=2: */
