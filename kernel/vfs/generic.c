#include <ananas/types.h>
#include <ananas/bio.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/trace.h>
#include <ananas/vfs.h>

TRACE_SETUP;

#undef DEBUG_VFS_LOOKUP

errorcode_t
vfs_generic_lookup(struct VFS_INODE* dirinode, struct VFS_INODE** destinode, const char* dentry)
{
	struct VFS_FILE dir;
	char buf[1024]; /* XXX */

#ifdef DEBUG_VFS_LOOKUP
	kprintf("vfs_generic_lookup: target='%s'\n", dentry);
#endif

	/*
	 * XXX This is a very naive implementation which does not use the
	 * possible directory index.
	 */
	KASSERT(S_ISDIR(dirinode->i_sb.st_mode), "supplied inode is not a directory");
	memset(&dir, 0, sizeof(dir));
	dir.f_inode = dirinode;
	while (1) {
		size_t buf_len = sizeof(buf);
		int result = vfs_read(&dir, buf, &buf_len);
		if (result != ANANAS_ERROR_NONE)
			return result;
		if (buf_len == 0)
			return ANANAS_ERROR(NO_FILE);

		char* cur_ptr = buf;
		while (buf_len > 0) {
			struct VFS_DIRENT* de = (struct VFS_DIRENT*)cur_ptr;
			buf_len -= DE_LENGTH(de); cur_ptr += DE_LENGTH(de);

#ifdef DEBUG_VFS_LOOKUP
			kprintf("vfs_generic_lookup('%s'): comparing with '%s'\n", dentry, de->de_fsop + de->de_fsop_length);
#endif
	
			if (strcmp(de->de_fsop + de->de_fsop_length, dentry) != 0)
				continue;

			/* Found it! */
			return vfs_get_inode(dirinode->i_fs, de->de_fsop, destinode);
		}
	}
}

/* vim:set ts=2 sw=2: */
