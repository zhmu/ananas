#include <ananas/types.h>
#include <ananas/bio.h>
#include <ananas/device.h>
#include <ananas/lib.h>
#include <ananas/vfs.h>

#undef DEBUG_VFS_LOOKUP

struct VFS_INODE*
vfs_generic_lookup(struct VFS_INODE* dirinode, const char* dentry)
{
	struct VFS_FILE dir;
	char tmp[1024]; /* XXX */

	/*
	 * XXX This is a very naive implementation which does not use the
	 * possible directory index.
	 */
	dir.offset = 0;
	dir.inode = dirinode;
	while (1) {
		size_t left = dirinode->iops->readdir(&dir, tmp, sizeof(tmp));
		if (left <= 0)
			break;

		char* cur_ptr = tmp;
		while (left > 0) {
			struct VFS_DIRENT* de = (struct VFS_DIRENT*)cur_ptr;
			left -= DE_LENGTH(de); cur_ptr += DE_LENGTH(de);

#ifdef DEBUG_VFS_LOOKUP
			kprintf("vfs_generic_lookup('%s'): comparing with '%s'\n", dentry, de->de_fsop + de->de_fsop_length);
#endif
	
			if (strcmp(de->de_fsop + de->de_fsop_length, dentry) != 0)
				continue;

			/* Found it! */
			struct VFS_INODE* inode = vfs_get_inode(dirinode->fs, de->de_fsop);
			if (inode == NULL)
				return NULL;
			return inode;
		}
	}

	/* Not found */
	return NULL;
}

/* vim:set ts=2 sw=2: */
