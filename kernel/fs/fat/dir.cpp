#include <ananas/types.h>
#include <ananas/bio.h>
#include <ananas/error.h>
#include <ananas/lock.h>
#include <ananas/trace.h>
#include <ananas/lib.h>
#include <ananas/vfs/types.h>
#include <ananas/vfs/core.h>
#include <ananas/vfs/generic.h>
#include <fat.h>
#include "block.h"
#include "dir.h"
#include "fatfs.h"

TRACE_SETUP;

#if 0
static void
fat_dump_entry(struct FAT_ENTRY* fentry)
{
	if (fentry->fe_attributes == FAT_ATTRIBUTE_LFN) {
		struct FAT_ENTRY_LFN* lfnentry = (struct FAT_ENTRY_LFN*)fentry;
		kprintf("lfilename  '");
		for (int i = 0; i < 5; i++)
			kprintf("%c", lfnentry->lfn_name_1[i * 2]);
		for (int i = 0; i < 6; i++)
			kprintf("%c", lfnentry->lfn_name_2[i * 2]);
		for (int i = 0; i < 2; i++)
			kprintf("%c", lfnentry->lfn_name_3[i * 2]);
		kprintf("' order %u\n", lfnentry->lfn_order);
	} else {
		kprintf("filename   '");
		for(int i = 0; i < 11; i++)
			kprintf("%c", fentry->fe_filename[i]);
		kprintf("'\n");
		kprintf("attributes 0x%x\n", fentry->fe_attributes);
		uint32_t cluster = FAT_FROM_LE16(fentry->fe_cluster_lo);
		cluster |= FAT_FROM_LE16(fentry->fe_cluster_hi) << 16; /* XXX FAT32 only */
		kprintf("cluster    %u\n", cluster);
		kprintf("size       %u\n", FAT_FROM_LE32(fentry->fe_size));
	}
}
#endif

/*
 * Used to construct a FAT filename. Returns zero if the filename is not
 * complete.
 */
static int
fat_construct_filename(struct FAT_ENTRY* fentry, char* fat_filename)
{
#define ADD_CHAR(c) do { \
		fat_filename[pos] = (c); \
		pos++; \
	} while(0)

	int pos = 0;
	if (fentry->fe_attributes == FAT_ATTRIBUTE_LFN) {
		struct FAT_ENTRY_LFN* lfnentry = (struct FAT_ENTRY_LFN*)fentry;

    /* LFN entries needn't be sequential, so fill them out as they pass by */
    pos = ((lfnentry->lfn_order & ~LFN_ORDER_LAST) - 1) * 13;

		/* LFN XXX should we bother about the checksum? */
		/* XXX we throw away the upper 8 bits */
		for (int i = 0; i < 5; i++)
			ADD_CHAR(lfnentry->lfn_name_1[i * 2]);
		for (int i = 0; i < 6; i++)
			ADD_CHAR(lfnentry->lfn_name_2[i * 2]);
		for (int i = 0; i < 2; i++)
			ADD_CHAR(lfnentry->lfn_name_3[i * 2]);
		return 0;
	}

	/*
	 * If we have a current LFN entry, we should use that instead and
	 * ignore the old 8.3 filename.
	 */
	if (fat_filename[0] != '\0')
			return 1;

	/* Convert the filename bits */
	for (int i = 0; i < 11; i++) {
		unsigned char ch = fentry->fe_filename[i];
		if (ch == 0x20)
			continue;
		if (i == 8) /* insert a '.' after the first 8 chars */
			ADD_CHAR('.');
		if (ch == 0x05) /* kanjii allows 0xe5 in filenames, so convert */
			ch = 0xe5;
		ADD_CHAR(ch);
	}

#undef ADD_CHAR

	return 1;
}

errorcode_t
fat_readdir(struct VFS_FILE* file, void* dirents, size_t* len)
{
	struct VFS_INODE* inode = file->f_dentry->d_inode;
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	size_t written = 0;
	size_t left = *len;
	char cur_filename[128]; /* currently assembled filename */
	off_t full_filename_offset = file->f_offset;
	struct BIO* bio = NULL;
	errorcode_t err = ananas_success();

	memset(cur_filename, 0, sizeof(cur_filename));

	blocknr_t cur_block = (blocknr_t)-1;
	while(left > 0) {
		/* Obtain the current directory block data */
		blocknr_t want_block;
		errorcode_t err = fat_block_map(inode, (file->f_offset / (blocknr_t)fs->fs_block_size), &want_block, 0);
		if (ANANAS_ERROR_CODE(err) == ANANAS_ERROR_BAD_RANGE)
			break;
		ANANAS_ERROR_RETURN(err);
		if (want_block != cur_block || bio == NULL) {
			if (bio != NULL) bio_free(bio);
			err = vfs_bread(fs, want_block, &bio);
			ANANAS_ERROR_RETURN(err);
			cur_block = want_block;
		}

		uint32_t cur_offs = file->f_offset % fs->fs_block_size;
		file->f_offset += sizeof(struct FAT_ENTRY);
		auto fentry = reinterpret_cast<struct FAT_ENTRY*>(static_cast<char*>(BIO_DATA(bio)) + cur_offs);
		if (fentry->fe_filename[0] == '\0') {
			/* First charachter is nul - this means there are no more entries to come */
			break;
		}

		if (fentry->fe_filename[0] == 0xe5) {
			/* Deleted file; we'll skip it */
			continue;
		}

		/* Convert the filename bits */
		if (!fat_construct_filename(fentry, cur_filename)) {
			/* This is part of a long file entry name - get more */
			continue;
		}

		/*
		 * If this is a volume entry, ignore it (but do this after the LFN has been handled
		 * because these will always have the volume bit set)
		 */
		if (fentry->fe_attributes & FAT_ATTRIBUTE_VOLUMEID)
			continue;

		/* And hand it to the fill function */
		uint64_t fsop = cur_block << 16 | cur_offs;
		int filled = vfs_filldirent(&dirents, &left, (const void*)&fsop, fs->fs_fsop_size, cur_filename, strlen(cur_filename));
		if (!filled) {
			/*
			 * Out of space - we need to restore the offset of the LFN chain. This must
			 * be done because we have assembled a full filename, only to find out that
			 * it does not fit; we need to do all this work again for the next readdir
		 	 * call.
			 */
			file->f_offset = full_filename_offset;
			break;
		}
		written += filled;
		/*
		 * Store the next offset; this is where our next filename starts (which
	 	 * does not have to fit in the destination buffer, so we'll have to
		 * read everything again)
		 */
		full_filename_offset = file->f_offset;

		/* Start over from the next filename */
		memset(cur_filename, 0, sizeof(cur_filename));
	}
	if (bio != NULL) bio_free(bio);
	*len = written;
	return err;
}

/*
 * Construct the 8.3-FAT notation for a given shortname and calculates the
 * checksum while doing so.
 */
static uint8_t
fat_sanitize_83_name(const char* fname, char* shortname)
{
	/* First, convert the filename to 8.3 form */
	int shortname_idx = 0;
	memset(shortname, ' ', 11);
	KASSERT(strlen(fname) <= 11, "shortname not short enough");
	for (int i = 0; i < strlen(fname); i++) {
		if (fname[i] == '.') {
			KASSERT(shortname_idx < 8, "multiple dots in shortname");
			shortname_idx = 8;
			continue;
		}
		shortname[shortname_idx++] = fname[i];
	}

	/* Calculate the checksum for LFN entries */
	uint8_t a = 0;
	for (int i = 0; i < 11; i++) {
		a  = ((a & 1) ? 0x80 : 0) | (a >> 1); /* a = a ror 1 */
		a += shortname[i];
	}
	return a;
}

/*
 * Adds a given FAT entry to a directory.
 *
 * This will update the name of the entry given. If dentry is NULL, no LFN name
 * will be generated.
 *
 * Note that we use uint32_t's because these are quicker to use and a directory
 * size cannot be >4GB anyway (MS docs claim most implementations use an
 * uint16_t!) - so relying on large FAT directories would be most unwise anyway.
 */
static errorcode_t
fat_add_directory_entry(struct VFS_INODE* dir, const char* dentry, struct FAT_ENTRY* fentry, void* fsop)
{
	struct VFS_MOUNTED_FS* fs = dir->i_fs;
	struct BIO* bio = NULL;

	/*
	 * Calculate the number of entries we need; each LFN entry stores 13 characters,
	 * and we'll need an entry for the file itself too. Note that we ALWAYS store
	 * a LFN name if a name is given, even if the 8.3 name would be sufficient.
	 */
	int chain_needed = 1;
	if (dentry != NULL)
		chain_needed += (strlen(dentry) + 12) / 13;

	blocknr_t cur_block = (blocknr_t)-1;
	uint32_t current_filename_offset = (uint32_t)-1;
	int	cur_lfn_chain = 0;
	uint32_t cur_dir_offset = 0;
	while(1) {
		/* Obtain the current directory block data */
		blocknr_t want_block;
		errorcode_t err = fat_block_map(dir, (cur_dir_offset / (blocknr_t)fs->fs_block_size), &want_block, 0);
		if (ANANAS_ERROR_CODE(err) == ANANAS_ERROR_BAD_RANGE) {
			/* We've hit an end-of-file - this means we'll have to enlarge the directory */
			cur_lfn_chain = -1;
			break;
		}
		ANANAS_ERROR_RETURN(err);
		if (want_block != cur_block || bio == NULL) {
			if (bio != NULL) bio_free(bio);
			err = vfs_bread(fs, want_block, &bio);
			ANANAS_ERROR_RETURN(err);
			cur_block = want_block;
		}

		/* See what this block's entry is all about */
		uint32_t cur_offs = cur_dir_offset % fs->fs_block_size;
		auto fentry = reinterpret_cast<struct FAT_ENTRY*>(static_cast<char*>(BIO_DATA(bio)) + cur_offs);
		if (fentry->fe_filename[0] == '\0') {
			/*
			 * First charachter is nul - this means there are no more entries to come
			 * and we can just write our entry here. Note that we assume there are no
			 * orphanaged LFN entries before here, which is conform the VFAT spec.
		 	 */
			current_filename_offset = cur_dir_offset;
			cur_lfn_chain = 0;
			break;
		}

		if (fentry->fe_attributes == FAT_ATTRIBUTE_LFN) {
			/*
			 * This entry is part of the LFN chain; store the offset if it's the
			 * first one as we may chose to re-use the entire chain.
			 */
			if (cur_lfn_chain++ == 0)
				current_filename_offset = cur_dir_offset;
			cur_dir_offset += sizeof(struct FAT_ENTRY);
			continue;
		}

		if (fentry->fe_filename[0] == 0xe5 && cur_lfn_chain >= chain_needed) {
			/*
			 * Entry here is deleted; this should come at the end of a LFN chain. If there
			 * is no chain, the check above will fail regardless and we skip this entry.
			  */
			break;
		}

		/*
		 * This isn't a LFN entry, nor is it a removed entry; we must skip over it.
		 */
		current_filename_offset = cur_dir_offset;
		cur_lfn_chain = 0;
		cur_dir_offset += sizeof(struct FAT_ENTRY);
	}

	/*
	 * With LFN, shortnames are actually not used so we can stash whatever we
	 * want there; it will only show up in tools that do not support LFN, which
	 * we truly don't care about. So instead of using MS' patented(!) shortname
	 * generation schema, we just roll our own - all that matters is that it's
	 * unique in the directory, so we use the offset.
	 */
	uint8_t shortname_checksum = 0;
	if (dentry != NULL) {
		char tmp_fname[12];
		sprintf(tmp_fname, "%u.LFN", current_filename_offset);
		shortname_checksum = fat_sanitize_83_name(tmp_fname, (char*)fentry->fe_filename);
	} /* else if (dentry == NULL) ... nothing to do, cur_entry_idx cannot be != chain_needed-1 */

	/*
	 * We've fallen out of our loop; there are three possible conditions (errors
	 * are handled directly and will not cause us to end up here):
	 *
	 * 1) cur_lfn_chain < 0  => Out of directory entries
	 *    The directory is to be enlarged and we can use its first entry.
	 * 2) cur_lfn_chain == 0 => End of directory
	 *    We can add the entry at cur_dir_offset
	 * 3) cur_lfn_chain > 0  => We've found entries we can overwrite
	 *    If this condition hits, there is enough space to place our entries.
	 */
	int filename_len = strlen(dentry);
	for(int cur_entry_idx = 0; cur_entry_idx < chain_needed; cur_entry_idx++) {
		/* Fetch/allocate the desired block */
		blocknr_t want_block;
		errorcode_t err = fat_block_map(dir, (current_filename_offset / (blocknr_t)fs->fs_block_size), &want_block, (cur_lfn_chain < 0));
		ANANAS_ERROR_RETURN(err);
		if (want_block != cur_block) {
			bio_free(bio);
			err = vfs_bread(fs, want_block, &bio);
			ANANAS_ERROR_RETURN(err);
			cur_block = want_block;
		}

		/* Fill out the FAT entry */
		uint32_t cur_offs = current_filename_offset % fs->fs_block_size;
		auto nentry = reinterpret_cast<struct FAT_ENTRY*>(static_cast<char*>(BIO_DATA(bio)) + cur_offs);
		if (cur_entry_idx == chain_needed - 1) {
			/* Final FAT entry; this contains the shortname */
			memcpy(nentry, fentry, sizeof(*nentry));
			/* fsop is the pointer to this entry */
			*(uint64_t*)fsop = cur_block << 16 | cur_offs;
		} else {
			/* LFN entry */
			struct FAT_ENTRY_LFN* lfn = (struct FAT_ENTRY_LFN*)nentry;
			memset(lfn, 0, sizeof(*lfn));
			lfn->lfn_order = cur_entry_idx + 1;
			if (cur_entry_idx + 2 == chain_needed)
				lfn->lfn_order |= LFN_ORDER_LAST;
			lfn->lfn_attribute = FAT_ATTRIBUTE_LFN;
			lfn->lfn_type = 0;
			lfn->lfn_checksum = shortname_checksum;

			/* Mangle the LFN entry in place */
			for (int i = 0; i < 5 && (13 * cur_entry_idx + i) < filename_len; i++)
				lfn->lfn_name_1[i * 2] = dentry[11 * cur_entry_idx + i];
			for (int i = 0; i < 6 && (13 * cur_entry_idx + 5 + i) < filename_len; i++)
				lfn->lfn_name_2[i * 2] = dentry[11 * cur_entry_idx + 5 + i];
			for (int i = 0; i < 2 && (13 * cur_entry_idx + 11 + i) < filename_len; i++)
				lfn->lfn_name_3[i * 2] = dentry[13 * cur_entry_idx + 11 + i];
		}

		bio_set_dirty(bio);
		current_filename_offset += sizeof(struct FAT_ENTRY);
	}

	bio_free(bio);
	return ananas_success();
}

static errorcode_t
fat_remove_directory_entry(struct VFS_INODE* dir, const char* dentry)
{
	struct VFS_MOUNTED_FS* fs = dir->i_fs;
	struct BIO* bio = NULL;

	char cur_filename[128]; /* currently assembled filename */
	memset(cur_filename, 0, sizeof(cur_filename));

	errorcode_t errorcode = ANANAS_ERROR(NO_FILE);
	blocknr_t cur_block = (blocknr_t)-1;
	uint32_t cur_dir_offset = 0;
	int chain_length = 0;
	while(1) {
		/* Obtain the current directory block data */
		blocknr_t want_block;
		errorcode_t err = fat_block_map(dir, (cur_dir_offset / (blocknr_t)fs->fs_block_size), &want_block, 0);
		if (ANANAS_ERROR_CODE(err) == ANANAS_ERROR_BAD_RANGE) {
			/* We've hit an end-of-file */
			break;
		}
		ANANAS_ERROR_RETURN(err);
		if (want_block != cur_block || bio == NULL) {
			if (bio != NULL) bio_free(bio);
			err = vfs_bread(fs, want_block, &bio);
			ANANAS_ERROR_RETURN(err);
			cur_block = want_block;
		}

		/* See what this block's entry is all about */
		uint32_t cur_offs = cur_dir_offset % fs->fs_block_size;
		cur_dir_offset += sizeof(struct FAT_ENTRY);
		auto fentry = reinterpret_cast<struct FAT_ENTRY*>(static_cast<char*>(BIO_DATA(bio)) + cur_offs);
		if (fentry->fe_filename[0] == '\0')
			break; /* final entry; bail out */
		if (fentry->fe_filename[0] == 0xe5) {
			chain_length = 0;
			continue; /* no entry or unused entry; skip it */
		}

		/* Convert the filename bits */
		chain_length++;
		if (!fat_construct_filename(fentry, cur_filename)) {
			/* This is part of a long file entry name - get more */
			continue;
		}

		/* XXX skip a volume label; we'll assume it cannot be removed */
		if (fentry->fe_attributes & FAT_ATTRIBUTE_VOLUMEID)
			continue;

		/* Okay, if the file matches what we intended to remove, we have a winner */
		if (strcmp(cur_filename, dentry) != 0) {
			/* Not a match; skip it */
			memset(cur_filename, 0, sizeof(cur_filename));
			chain_length = 0;
			continue;
		}

		/*
		 * Entry is found; we need to remove it. As we know the chain length of LFN pieces, we have to
		 * rewind to the first one and throw them all away.
		 */
		KASSERT(chain_length > 0, "found '%s' without sensible chain length (got %d)", cur_filename, chain_length);
		cur_dir_offset -= sizeof(struct FAT_ENTRY) * chain_length;

		for(/* nothing */; chain_length > 0; chain_length--) {
			/* Obtain the current directory block data */
			blocknr_t want_block;
			errorcode_t err = fat_block_map(dir, (cur_dir_offset / (blocknr_t)fs->fs_block_size), &want_block, 0);
			if (ANANAS_ERROR_CODE(err) == ANANAS_ERROR_BAD_RANGE) {
				/* We've hit an end-of-file */
				break;
			}
			ANANAS_ERROR_RETURN(err);
			if (want_block != cur_block || bio == NULL) {
				if (bio != NULL) bio_free(bio);
				err = vfs_bread(fs, want_block, &bio);
				ANANAS_ERROR_RETURN(err);
				cur_block = want_block;
			}

			/* Update the FAT entry here; the file name is removed */
			uint32_t cur_offs = cur_dir_offset % fs->fs_block_size;
			cur_dir_offset += sizeof(struct FAT_ENTRY);
			auto fentry = reinterpret_cast<struct FAT_ENTRY*>(static_cast<char*>(BIO_DATA(bio)) + cur_offs);
			fentry->fe_filename[0] = 0xe5; /* deleted */

			bio_set_dirty(bio);
		}
		errorcode = ananas_success();
		break;
	}

	if (bio != NULL)
		bio_free(bio);
	return errorcode;
}

static errorcode_t
fat_create(struct VFS_INODE* dir, struct DENTRY* de, int mode)
{
	struct FAT_ENTRY fentry;
	memset(&fentry, 0, sizeof(fentry));
	fentry.fe_attributes = FAT_ATTRIBUTE_ARCHIVE; /* XXX must be specifiable */

	/* Hook the new file to the directory */
	uint64_t fsop;
	errorcode_t err = fat_add_directory_entry(dir, de->d_entry, &fentry, (void*)&fsop);
	ANANAS_ERROR_RETURN(err);

	/* And obtain it */
	struct VFS_INODE* inode;
	err = vfs_get_inode(dir->i_fs, &fsop, &inode);
	ANANAS_ERROR_RETURN(err);

	/* Almost done - hook it to the dentry */
	dcache_set_inode(de, inode);
	return err;
}

static errorcode_t
fat_unlink(struct VFS_INODE* dir, struct DENTRY* de)
{
	/* Sanity checks first: we must have a backing inode */
	if (de->d_inode == NULL || de->d_flags & DENTRY_FLAG_NEGATIVE)
		return ANANAS_ERROR(BAD_OPERATION);

	/*
	 * We must remove this item from the directory it is in - the nlink field determines when the file data
	 * itself will go.
	 */
	KASSERT(de->d_inode->i_sb.st_nlink > 0, "removing entry '%s' with invalid link %d", de->d_entry, de->d_inode->i_sb.st_nlink);
	errorcode_t err = fat_remove_directory_entry(dir, de->d_entry);
	ANANAS_ERROR_RETURN(err);

	/*
	 * All is well; decrement the nlink field - if it reaches zero, the file
	 * content will be removed once the inode is destroyed
	 */
	de->d_inode->i_sb.st_nlink--;
	vfs_set_inode_dirty(de->d_inode);
	return ananas_success();
}

static errorcode_t
fat_rename(struct VFS_INODE* old_dir, struct DENTRY* old_dentry, struct VFS_INODE* new_dir, struct DENTRY* new_dentry)
{
	KASSERT(!S_ISDIR(old_dentry->d_inode->i_sb.st_mode), "FIXME directory");

	/*
	 * Due to the way we use FSOP's (it is the location within the directory), we
	 * need to alter the inode itself to refer to the new location - to do this,
	 * we'll create a blank item in the new directory and copy the relevant
	 * inode fields over .
	 */

	/* First step: create the new name in the new directory */
	struct FAT_ENTRY fentry;
	memset(&fentry, 0, sizeof(fentry));
	fentry.fe_attributes = FAT_ATTRIBUTE_ARCHIVE; /* XXX we should copy the old entry */

	uint64_t new_fsop;
	errorcode_t err = fat_add_directory_entry(new_dir, new_dentry->d_entry, &fentry, (void*)&new_fsop);
	ANANAS_ERROR_RETURN(err);

	/* And fetch the new inode */
	struct VFS_INODE* inode;
	err = vfs_get_inode(new_dir->i_fs, &new_fsop, &inode);
	if (ananas_is_failure(err)) {
		fat_remove_directory_entry(new_dir, new_dentry->d_entry); /* XXX hope this works! */
		return err;
	}

	/* Get rid of the previous directory entry */
	err = fat_remove_directory_entry(old_dir, old_dentry->d_entry);
	if (ananas_is_failure(err)) {
		vfs_deref_inode(inode); /* remove the previous inode */
		fat_remove_directory_entry(new_dir, new_dentry->d_entry); /* XXX hope this works! */
		return err;
	}

	/*
	 * Copy the inode information over; the old inode will soon go XXX we should copy more
	 */
	struct VFS_INODE* old_inode = old_dentry->d_inode;
	inode->i_sb.st_size = old_inode->i_sb.st_size;
	memcpy(inode->i_privdata, old_inode->i_privdata, sizeof(struct FAT_INODE_PRIVDATA));
	vfs_set_inode_dirty(inode);

	/*
	 * Okay, the on-disk structure is okay; update the dentries. This should
	 * abandon the previous inode (as the old ref will be freed)
	 */
	dcache_set_inode(old_dentry, inode);
	dcache_set_inode(new_dentry, inode);
	return ananas_success();
}

struct VFS_INODE_OPS fat_dir_ops = {
	.readdir = fat_readdir,
	.lookup = vfs_generic_lookup,
	.create = fat_create,
	.unlink = fat_unlink,
	.rename = fat_rename,
};

/* vim:set ts=2 sw=2: */
