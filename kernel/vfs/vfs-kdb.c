#include <ananas/types.h>
#include <ananas/vfs/core.h>
#include <ananas/vfs/dentry.h>
#include <ananas/vfs/mount.h>
#include <ananas/vfs/icache.h>
#include <ananas/kdb.h>
#include <ananas/thread.h>
#include <ananas/lib.h>

extern struct THREAD* threads;

/*
 * This function is more of a sanity check; it does not lock anything to
 * prevent deadlocks.
 */
void
kdb_cmd_inodes(int num_args, char** arg)
{
	struct VFS_MOUNTED_FS* fs = vfs_get_rootfs(); /* XXX only root for now */

	DQUEUE_FOREACH(&fs->fs_icache_inuse, ii, struct ICACHE_ITEM) {
		int expected_refs = 1; /* icache */
		kprintf("inode=%p, fsop=", ii->inode);
		for (int i = 0; i < fs->fs_fsop_size; i++) {
			if (i > 0) kprintf(" ");
			kprintf("%x", (unsigned char)ii->fsop[i]);
		}
		if (ii->inode == NULL) {
			kprintf(", nil\n");
			continue;
		}
	//	if (ii->inode == fs->fs_root_inode) expected_refs++; /* root inode */
		kprintf(", refcount=%u", ii->inode->i_refcount);
		const char* dentry_name = "?";
		DQUEUE_FOREACH(&fs->fs_dcache_inuse, d, struct DENTRY) {
			if (d->d_inode != ii->inode && d->d_parent->d_inode != ii->inode)
				continue;
			if (d->d_inode == ii->inode) {
				dentry_name = d->d_entry;
				expected_refs++; /* dentry entry ref */
			}
			if (d->d_parent->d_inode == ii->inode) {
				expected_refs++; /* dentry dir ref */
			}
		}
		kprintf(", dentry='%s'\n", dentry_name);

#ifdef NOTYET
		/* Now scour the handles for references to this inode */
		for (struct THREAD* t = threads; t != NULL; t = t->next) {
			if(!DQUEUE_EMPTY(&t->handles)) {
				DQUEUE_FOREACH_SAFE(&t->handles, handle, struct HANDLE) {
					if (handle->type != HANDLE_TYPE_FILE)
						continue;
					if (handle->data.vfs_file.f_inode == ii->inode)
						expected_refs++; /* handle ref */
				}
			}
		}

		if (expected_refs != ii->inode->i_refcount)
			kprintf("WARNING: has %u refs, expected %u!!!\n",
			 ii->inode->i_refcount, expected_refs);
#endif
	}		
}

/* vim:set ts=2 sw=2: */
