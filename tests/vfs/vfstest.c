#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ananas/error.h>
#include <ananas/bio.h>
#include <ananas/vfs.h>

/* Files to use for testing */
#define FILE1 "vfstest"
#define FILE2 "doesnotexist"
#define DEVFS_MOUNTPOINT "dev" /* FILE3 */
#define FILE4 "notthere"
#define FILENAME_NONE "doesnotexist.%u"
#define FILENAME_DUMMY "%02i"

#define CHECK_OK(x) \
	check_err((x), ANANAS_ERROR_NONE, STRINGIFY(x))

#define CHECK_ERROR(x,e) \
	check_err((x), ANANAS_ERROR_##e, STRINGIFY(x))

char* vfstest_fsimage = NULL;

static void
check_err(errorcode_t err, errorcode_t e, const char* func)
{
	if (ANANAS_ERROR_CODE(err) == e)
		return;
	printf("%s: failed, error code %u\n", func, err);
	abort();
}

#define CALCULATE_ICACHE_SIZE(fs,i) \
	do { \
		(i) = 0; \
		if(!DQUEUE_EMPTY(&(fs)->fs_icache_inuse)) \
			DQUEUE_FOREACH(&(fs)->fs_icache_inuse, ii, struct ICACHE_ITEM) { (i)++; } \
	} while(0)

#define CALCULATE_DCACHE_SIZE(fs,i) \
	do { \
		(i) = 0; \
		if(!DQUEUE_EMPTY(&(fs)->fs_dcache_inuse)) \
			DQUEUE_FOREACH(&(fs)->fs_dcache_inuse, di, struct DENTRY_CACHE_ITEM) { (i)++; } \
	} while(0)

int
main(int argc, char* argv[])
{
	int icache_size, dcache_size;
	struct ICACHE_ITEM* ii;
	struct DENTRY_CACHE_ITEM* di;

	if (argc != 2) {
		printf("usage: vfstest image.ext2\n");
		return 1;
	}
	vfstest_fsimage = argv[1];

	/* Give the subsystems a go, as we depend on them */
	device_init();
	bio_init();
	vfs_init();

	/*
	 * For now, we assume we can store at least a many entries in the dentry
	 * cache than in the actual inode cache; this makes sense as we cache
	 * negative lookups as well.
	 *
	 * This property is used in part 3 of the tests.
	 */
	assert(DCACHE_ITEMS_PER_FS >= ICACHE_ITEMS_PER_FS);

	/*
	 * Part 1: Basic mounting, inodes, inode refcounts and dentry checks.
	 */
	
	/* Mount a root filesystem */
	CHECK_OK(vfs_mount("vfile", "/", "ext2", NULL));

	/* Obtain an inode to the root */
	struct VFS_INODE* root_inode;
	CHECK_OK(vfs_lookup(NULL, &root_inode, "/"));

	/* There should now be a single inode in the cache */
	CALCULATE_ICACHE_SIZE(root_inode->i_fs, icache_size);
	assert(icache_size == 1);

	/* The entry must be identical to the root_inode and filesystem root inode */
	ii = DQUEUE_HEAD(&root_inode->i_fs->fs_icache_inuse);
	assert(ii->inode == root_inode);
	assert(ii->inode == root_inode->i_fs->fs_root_inode);

	/*
	 * Refcount must be 3 (fs->root_inode, root_inode and cache) - it will not be
 	 * in the dentry cache
	 */
	assert(ii->inode->i_refcount == 3);

	/* Thus, there should be nothing in the dentry cache */
	CALCULATE_DCACHE_SIZE(root_inode->i_fs, dcache_size);
	assert(dcache_size == 0);

	/* Now, open a file in the root */
	struct VFS_FILE file1;
	CHECK_OK(vfs_open(FILE1, NULL, &file1));

	/* There should now be two entries in the inode cache... */
	CALCULATE_ICACHE_SIZE(root_inode->i_fs, icache_size);
	assert(icache_size == 2);
	/* First is the file we just opened (as it is most recently used) */
	ii = DQUEUE_HEAD(&root_inode->i_fs->fs_icache_inuse);
	assert(ii->inode == file1.f_inode);
	/* With a refcount of 3 (file, dentry cache, inode cache) */
	assert(ii->inode->i_refcount == 3);
	/* ...second is the root inode ... */
	ii = DQUEUE_NEXT(ii);
	assert(ii->inode == root_inode);
	/* ... with a refcount of 4 (fs->root_inode, root_inode, dentry.parent and cache) */
	assert(root_inode->i_refcount == 4);

	/* There should be a single item in the entry cache... */
	CALCULATE_DCACHE_SIZE(root_inode->i_fs, dcache_size);
	assert(dcache_size == 1);

	/* ... namely the file we opened */
	di = DQUEUE_HEAD(&root_inode->i_fs->fs_dcache_inuse);
	assert(di->d_dir_inode == root_inode);
	assert(di->d_entry_inode == file1.f_inode);
	assert(strcmp(di->d_entry, FILE1) == 0);
	/* And it must not be a negative entry */
	assert((di->d_flags & DENTRY_FLAG_NEGATIVE) == 0);

	/*
	 * Let's try to read our test file and compare it. We read in oddly-sized
	 * chunks to cause extra strain in the VFS/BIO layers.
	 */
#define CHUNK_LEN 987
	char source_data[CHUNK_LEN], dest_data[CHUNK_LEN];
	FILE* f = fopen(FILE1, "rb");
	if (f == NULL) {
		printf("cannot open '%s' in local directory\n", FILE1);
		abort();
	}
	while (1) {
		size_t source_len = fread(source_data, 1, CHUNK_LEN, f);
		size_t dest_len   = CHUNK_LEN;
 		CHECK_OK(vfs_read(&file1, dest_data, &dest_len));
		if (source_len != dest_len) {
			printf("read length mismatch, source=%zu, dest=%zu\n", source_len, dest_len);
			abort();
		}
		if (source_len == 0)
			/* end of file! */
			break;

		if (memcmp(source_data, dest_data, source_len) != 0) {
			printf("read data mismatch\n");
			abort();
		}
	}
	fclose(f);

	/* Now, we close the VFS file */
	struct VFS_INODE* file1inode = file1.f_inode;
	CHECK_OK(vfs_close(&file1));

	/* Now, the backing inode must still exist in the cache... */
	CALCULATE_ICACHE_SIZE(root_inode->i_fs, icache_size);
	assert(icache_size == 2);
	ii = DQUEUE_HEAD(&root_inode->i_fs->fs_icache_inuse);
	assert(ii->inode == file1inode);
	/* ... but the refcount must be only one (inode cache, dentry cache) */
	assert(file1inode->i_refcount == 2);

	/* Try to open a nonexistant file */
	struct VFS_FILE file2;
	CHECK_ERROR(vfs_open(FILE2, NULL, &file2), NO_FILE);

	/* The inode cache mustn't have changed */
	CALCULATE_ICACHE_SIZE(root_inode->i_fs, icache_size);
	assert(icache_size == 2);

	/* The entry cache must have an extra item... */
	CALCULATE_DCACHE_SIZE(root_inode->i_fs, dcache_size);
	assert(dcache_size == 2);

	/* ...namely our lookup... */
	di = DQUEUE_HEAD(&root_inode->i_fs->fs_dcache_inuse);
	assert(di->d_dir_inode == root_inode);
	assert(strcmp(di->d_entry, FILE2) == 0);

	/* ...which must have failed */
	assert(di->d_entry_inode == NULL);
	assert((di->d_flags & DENTRY_FLAG_NEGATIVE) != 0);

	/* Regardless, the root inode's refcount must be one higher (+dentry dirinode) */
	assert(root_inode->i_refcount == 5);

	/*
	 * Part 2: Nested filesystems check.
	 */

	/* Mount a device filesystem on top of our filesystem */
	CHECK_OK(vfs_mount(NULL, "/" DEVFS_MOUNTPOINT, "devfs", NULL));

	/* We must still have 2 inodes in the root filesystem's inode cache... */
	CALCULATE_ICACHE_SIZE(root_inode->i_fs, icache_size);
	assert(icache_size == 2);

	/* The first is the file inode... */
	ii = DQUEUE_HEAD(&root_inode->i_fs->fs_icache_inuse);
	/* ...with 2 refs (dentry cache, inode cache) */
	assert(ii->inode->i_refcount == 2);
	/* And then the root inode... */
	ii = DQUEUE_NEXT(ii);
	assert(ii->inode == root_inode);
	/* ...with a refcount of 6 (5 + dentry for /dev dir)... */
	assert(ii->inode->i_refcount == 6);

	/*
	 * Obtain the new filesystem's root inode; we can't check the inode or entry
 	 * cache without it.
	 */
	struct VFS_FILE file3;
	CHECK_OK(vfs_open(DEVFS_MOUNTPOINT, NULL, &file3));

	/* The filesystem must have a single inode in cache... */
	CALCULATE_ICACHE_SIZE(file3.f_inode->i_fs, icache_size);
	assert(icache_size == 1);

	/* ...and it should be our inode, the root one... */
	ii = DQUEUE_HEAD(&file3.f_inode->i_fs->fs_icache_inuse);
	assert(ii->inode == file3.f_inode);
	assert(ii->inode == file3.f_inode->i_fs->fs_root_inode);

	/* ...and it must have 3 refs (file3, fs->root_inode, cache) */
	assert(ii->inode->i_refcount == 3);

	/* The entry cache of the new filesystem must be empty */
	CALCULATE_DCACHE_SIZE(file3.f_inode->i_fs, dcache_size);
	assert(dcache_size == 0);

	/* The root filesystem must have 3 entries in the inode cache... */
	CALCULATE_DCACHE_SIZE(root_inode->i_fs, dcache_size);
	assert(dcache_size == 3);

	/* ...where the first one is our mountpoint... */
	di = DQUEUE_HEAD(&root_inode->i_fs->fs_dcache_inuse);
	assert(di->d_dir_inode == root_inode);
	assert(di->d_entry_inode == file3.f_inode);
	assert(strcmp(di->d_entry, DEVFS_MOUNTPOINT) == 0);
	/* ...it must not be a negative entry... */
	assert((di->d_flags & DENTRY_FLAG_NEGATIVE) == 0);
	/* ...but marked permant */
	assert((di->d_flags & DENTRY_FLAG_PERMANENT) != 0);

	/* Check the refcount of the root filesystem; it must not have changed */	
	CALCULATE_ICACHE_SIZE(root_inode->i_fs, icache_size);
	assert(icache_size == 2);
	ii = DQUEUE_HEAD(&root_inode->i_fs->fs_icache_inuse);
	/* First is the file inode... */
	assert(ii->inode == file1inode);
	/* ... but the refcount must be two (inode cache, dentry cache) */
	assert(file1inode->i_refcount == 2);
	ii = DQUEUE_NEXT(ii);
	/* Then the root inode ... */
	assert(ii->inode == root_inode);
	/* ... refcount must be 6 (fs->root_inode, root_inode, cache, 3x dentry parent dir) */
	assert(ii->inode->i_refcount == 6);

	/*
	 * Part 3: Overload conditions.
	 */

	/*
	 * First, overflow the root filesystem's dentry cache by requesting a bunch of
	 * nonexisting files. Before we do so, ensure the cache consists of 3 entries,
	 * FILE1, FILE2 and DEVFS_MOUNTPOINT (even though all files are closed)
	 * 
	 */
	CALCULATE_DCACHE_SIZE(root_inode->i_fs, dcache_size);
	assert(dcache_size == 3);
	di = DQUEUE_HEAD(&root_inode->i_fs->fs_dcache_inuse);
	/* First entry must be DEFS_MOUNTPOINT in the root (most recently used)... */
	assert(di->d_dir_inode == root_inode);
	assert(strcmp(di->d_entry, DEVFS_MOUNTPOINT) == 0);
	/* ...which is present and permanent */
	assert((di->d_flags & DENTRY_FLAG_NEGATIVE) == 0);
	assert((di->d_flags & DENTRY_FLAG_PERMANENT) != 0);
	di = DQUEUE_NEXT(di);
	/* Second entry is FILE2 in the root... */
	assert(di->d_dir_inode == root_inode);
	assert(strcmp(di->d_entry, FILE2) == 0);
	/* ...which is negative but not permanent */
	assert((di->d_flags & DENTRY_FLAG_NEGATIVE) != 0);
	assert((di->d_flags & DENTRY_FLAG_PERMANENT) == 0);
	di = DQUEUE_NEXT(di);
	/* Final entry is FILE1 in the root... */
	assert(di->d_dir_inode == root_inode);
	assert(strcmp(di->d_entry, FILE1) == 0);
	/* ...which is neither negative nor permanent */
	assert((di->d_flags & DENTRY_FLAG_NEGATIVE) == 0);
	assert((di->d_flags & DENTRY_FLAG_PERMANENT) == 0);

	/* Now, fill the dentry cache to the brim */
	for (int i = 3; i < DCACHE_ITEMS_PER_FS; i++) {
		struct VFS_FILE file;
		char filename[64 /* XXX */];
		snprintf(filename, sizeof(filename), FILENAME_NONE, i);
		CHECK_ERROR(vfs_open(filename, NULL, &file), NO_FILE);
	}

	/* OK, dentry cache must be filled completely now */
	CALCULATE_DCACHE_SIZE(root_inode->i_fs, dcache_size);
	assert(dcache_size == DCACHE_ITEMS_PER_FS);

	/* Namely with FILENAME_NONE[DCACHE_ITEMS_PER_FS .. 3] entries */
	di = DQUEUE_HEAD(&root_inode->i_fs->fs_dcache_inuse);
	for (int i = DCACHE_ITEMS_PER_FS - 1; i >= 3; i--) {
		/* Which have the correct filename... */
		char filename[64 /* XXX */];
		snprintf(filename, sizeof(filename), FILENAME_NONE, i);
		assert(strcmp(di->d_entry, filename) == 0);
		/* ...and are negative yet not permanent */
		assert((di->d_flags & DENTRY_FLAG_NEGATIVE) != 0);
		assert((di->d_flags & DENTRY_FLAG_PERMANENT) == 0);
		di = DQUEUE_NEXT(di);
	}

	/*
	 * Now we'll expect to see the mountpoint entry. If it's there, we don't bother
	 * to check the other entries because they'll likely be there.
	 */
	assert(di->d_dir_inode == root_inode);
	assert(strcmp(di->d_entry, DEVFS_MOUNTPOINT) == 0);
	assert((di->d_flags & DENTRY_FLAG_NEGATIVE) == 0);
	assert((di->d_flags & DENTRY_FLAG_PERMANENT) != 0);

	/* Check the inode cache; there must only be two items in there */
	CALCULATE_ICACHE_SIZE(root_inode->i_fs, icache_size);
	assert(icache_size == 2);
	ii = DQUEUE_HEAD(&root_inode->i_fs->fs_icache_inuse);
	/* First the file inode... */
	assert(ii->inode == file1inode);
	/* ... but the refcount must remain at two (inode cache, dentry cache) */
	assert(file1inode->i_refcount == 2);
	ii = DQUEUE_NEXT(ii);
	/* And then the root inode... */
	assert(ii->inode == root_inode);
	/* ... refcount must be 3 (fs->root_inode, root_inode, inode cache) +  #cache_items */
	assert(ii->inode->i_refcount == 3 + DCACHE_ITEMS_PER_FS);

	/* Now, let's request yet another nonexistent file ... */
	struct VFS_FILE file4;
	CHECK_ERROR(vfs_open(FILE4, NULL, &file4), NO_FILE);

	/* ... the dentry cache size cannot have changed */
	CALCULATE_DCACHE_SIZE(root_inode->i_fs, dcache_size);
	assert(dcache_size == DCACHE_ITEMS_PER_FS);

	/* We expect our first entry to be the file we just asked for... */
	di = DQUEUE_HEAD(&root_inode->i_fs->fs_dcache_inuse);
	assert(di->d_dir_inode == root_inode);
	assert(strcmp(di->d_entry, FILE4) == 0);
	/* ...which is negative but not permanent */
	assert((di->d_flags & DENTRY_FLAG_NEGATIVE) != 0);
	assert((di->d_flags & DENTRY_FLAG_PERMANENT) == 0);
	di = DQUEUE_NEXT(di);

	/* The next entries entry should be the files we asked for in the loop... */
	for (int i = DCACHE_ITEMS_PER_FS - 1; i >= 3; i--) {
		/* Which have the correct filename... */
		char filename[64 /* XXX */];
		snprintf(filename, sizeof(filename), FILENAME_NONE, i);
		assert(strcmp(di->d_entry, filename) == 0);
		/* ...and are negative yet not permanent */
		assert((di->d_flags & DENTRY_FLAG_NEGATIVE) != 0);
		assert((di->d_flags & DENTRY_FLAG_PERMANENT) == 0);
		di = DQUEUE_NEXT(di);
	}

	/* The pre-final entry must be the device filesystem entry... */
	assert(di->d_dir_inode == root_inode);
	assert(strcmp(di->d_entry, DEVFS_MOUNTPOINT) == 0);
	assert((di->d_flags & DENTRY_FLAG_NEGATIVE) == 0);
	assert((di->d_flags & DENTRY_FLAG_PERMANENT) != 0);
	di = DQUEUE_NEXT(di);

	/* And finally, we have FILE1 (because we prefer to remove non-negative entries) */
	assert(strcmp(di->d_entry, FILE1) == 0);
	assert((di->d_flags & DENTRY_FLAG_NEGATIVE) == 0);
	assert((di->d_flags & DENTRY_FLAG_PERMANENT) == 0);
	assert(DQUEUE_NEXT(di) == NULL); /* just a safeguard to verify the logic above */

	/*
	 * As we have only requested non-present inodes, the inode cache must not
	 * have changed (except for the file1 inode, which has only a single ref
	 * now)
	 */
	CALCULATE_ICACHE_SIZE(root_inode->i_fs, icache_size);
	assert(icache_size == 2);

	ii = DQUEUE_HEAD(&root_inode->i_fs->fs_icache_inuse);
	/* First the file inode.. .*/
	assert(ii->inode == file1inode);
	/* ... yet file1's refcount must be two (inode and dentry cache) */
	assert(file1inode->i_refcount == 2);
	ii = DQUEUE_NEXT(ii);
	/* Then the root inode... */
	assert(ii->inode == root_inode);
	/* .. .refcount must be 3 (fs->root_inode, root_inode, inode cache) + #cache_items */
	assert(ii->inode->i_refcount == 3 + DCACHE_ITEMS_PER_FS);

	/*
	 * Fill the inode cache by requesting files that actually exist; we
	 * immediately close them afterwards. This may cause the dentry cache
	 * to overflow, but that is alright as we verified it above here.
	 */
	for (int i = 0; i < ICACHE_ITEMS_PER_FS - 2; i++) {
		struct VFS_FILE file;
		char filename[64 /* XXX */];
		snprintf(filename, sizeof(filename), FILENAME_DUMMY, i);
		CHECK_OK(vfs_open(filename, NULL, &file));
		CHECK_OK(vfs_close(&file));
	}

	/* Inode cache must be completely filled now */
	CALCULATE_ICACHE_SIZE(root_inode->i_fs, icache_size);
	assert(icache_size == ICACHE_ITEMS_PER_FS);

	/*
	 * Verify the dentry cache - this uses the fact that DCACHE_ITEMS_PER_FS >=
	 * ICACHE_ITEMS_PER_FS; all of our inode items have to be in the dentry cache
	 * because the latter is larger.
	 */
	di = DQUEUE_HEAD(&root_inode->i_fs->fs_dcache_inuse);
	for (int i = ICACHE_ITEMS_PER_FS - 3; i >= 0; i--) {
		char filename[64 /* XXX */];
		snprintf(filename, sizeof(filename), FILENAME_DUMMY, i);
		assert(strcmp(di->d_entry, filename) == 0);
		/* Entry should be present and not permanent */
		assert((di->d_flags & DENTRY_FLAG_NEGATIVE) == 0);
		assert((di->d_flags & DENTRY_FLAG_PERMANENT) == 0);
		assert(di->d_dir_inode == root_inode);
		assert(di->d_entry_inode != NULL);
		/*
		 * Ensure there is exactly one entry in the inode cache for this item; this
		 * ensures the inode cache is valid.
		 */
		int found = 0;
		DQUEUE_FOREACH(&root_inode->i_fs->fs_icache_inuse, ii, struct ICACHE_ITEM) {
			if (ii->inode == di->d_entry_inode)
				found++;
		}
		assert(found == 1);
		di = DQUEUE_NEXT(di);
	}

	/*
	 * OK - inode cache is fully filled, dentry cache is fully filled. Now, fill
	 * the entire dentry cache with requests. Because it is at least the inode
	 * cache size, this cannot be satisfied.
	 */
	for (int i = 0; i < DCACHE_ITEMS_PER_FS; i++) {
		struct VFS_FILE file;
		char filename[64 /* XXX */];
		snprintf(filename, sizeof(filename), FILENAME_DUMMY, i);
		CHECK_OK(vfs_open(filename, NULL, &file));
		CHECK_OK(vfs_close(&file));
	}

	/* Inode cache must be completely filled now... */
	CALCULATE_ICACHE_SIZE(root_inode->i_fs, icache_size);
	assert(icache_size == ICACHE_ITEMS_PER_FS);

	/*
	 * ... yet we can't expect the dentry cache to be completely filled,
	 * because the inode code is free to clear it; all we can do is check
	 * that at least the files we opened are in there, but as the test
	 * filesystem image isn't specially generated, matching them is quite
	 * hard - so we just have to settle for a check that at least the
	 * cache entries are in there.
	 *
	 */
	CALCULATE_DCACHE_SIZE(root_inode->i_fs, dcache_size);
	assert(dcache_size >= ICACHE_ITEMS_PER_FS);
	for (int i = 0; i < DCACHE_ITEMS_PER_FS; i++) {
		struct VFS_FILE file;
		char filename[64 /* XXX */];
		snprintf(filename, sizeof(filename), FILENAME_DUMMY, i);

		/* See if the filename lives in the dentry cache; it should because the latter is larger */
		int found = 0;
		DQUEUE_FOREACH(&root_inode->i_fs->fs_dcache_inuse, di, struct DENTRY_CACHE_ITEM) {
			if (strcmp(di->d_entry, filename) != 0)
				continue;
			/* Got it; it must be non-negative and non-permanent */
			assert((di->d_flags & DENTRY_FLAG_NEGATIVE) == 0);
			assert((di->d_flags & DENTRY_FLAG_PERMANENT) == 0);
			found++;
		}
		assert(found == 1);
	}

	return 0;
}

/* vim:set ts=2 sw=2: */
