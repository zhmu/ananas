#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ananas/error.h>
#include <ananas/bio.h>
#include <ananas/vfs.h>

/* Files to use for testing */
#define FILE1 "kernel"
#define FILE2 "doesnotexist"

#define DEVFS_MOUNTPOINT "dev"

#define CHECK_OK(x) \
	check_err((x), ANANAS_ERROR_NONE, STRINGIFY(x))

#define CHECK_ERROR(x,e) \
	check_err((x), ANANAS_ERROR_##e, STRINGIFY(x))

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
		if(!DQUEUE_EMPTY(&(fs)->icache_inuse)) \
			DQUEUE_FOREACH(&(fs)->icache_inuse, ii, struct ICACHE_ITEM) { (i)++; } \
	} while(0)

#define CALCULATE_DCACHE_SIZE(fs,i) \
	do { \
		(i) = 0; \
		if(!DQUEUE_EMPTY(&(fs)->dcache_inuse)) \
			DQUEUE_FOREACH(&(fs)->dcache_inuse, di, struct DENTRY_CACHE_ITEM) { (i)++; } \
	} while(0)

int
main(int argc, char* argv[])
{
	int icache_size, dcache_size;
	struct ICACHE_ITEM* ii;
	struct DENTRY_CACHE_ITEM* di;

	/* Give the subsystems a go, as we depend on them */
	device_init();
	bio_init();
	vfs_init();

	/*
	 * Part 1: Basic mounting, inodes, inode refcounts and dentry checks.
	 */
	
	/* Mount a root filesystem */
	CHECK_OK(vfs_mount("vfile", "/", "ext2", NULL));

	/* Obtain an inode to the root */
	struct VFS_INODE* root_inode;
	CHECK_OK(vfs_lookup(NULL, &root_inode, "/"));

	/* There should now be a single inode in the cache */
	CALCULATE_ICACHE_SIZE(root_inode->fs, icache_size);
	assert(icache_size == 1);

	/* The entry must be identical to the root_inode and filesystem root inode */
	ii = DQUEUE_HEAD(&root_inode->fs->icache_inuse);
	assert(ii->inode == root_inode);
	assert(ii->inode == root_inode->fs->root_inode);

	/* Refcount must be 3 (fs->root_inode, root_inode and cache) */
	assert(ii->inode->refcount == 3);

	/* There should be nothing in the dentry cache */
	CALCULATE_DCACHE_SIZE(root_inode->fs, dcache_size);
	assert(dcache_size == 0);

	/* Now, open a file in the root */
	struct VFS_FILE file1;
	CHECK_OK(vfs_open(FILE1, NULL, &file1));

	/* There should now be two entries in the inode cache... */
	CALCULATE_ICACHE_SIZE(root_inode->fs, icache_size);
	assert(icache_size == 2);
	/* ... first is the root inode ... */
	ii = DQUEUE_HEAD(&root_inode->fs->icache_inuse);
	assert(ii->inode == root_inode);
	/* ... and second is the file we just opened */
	ii = DQUEUE_NEXT(ii);
	assert(ii->inode == file1.inode);
	/* With a refcount of 2 (file, cache) */
	assert(ii->inode->refcount == 2);

	/* There should be a single item in the entry cache... */
	CALCULATE_DCACHE_SIZE(root_inode->fs, dcache_size);
	assert(dcache_size == 1);

	/* ... namely the file we opened */
	di = DQUEUE_HEAD(&root_inode->fs->dcache_inuse);
	assert(di->d_dir_inode == root_inode);
	assert(di->d_entry_inode == file1.inode);
	assert(strcmp(di->d_entry, FILE1) == 0);
	/* And it must not be a negative entry */
	assert((di->d_flags & DENTRY_FLAG_NEGATIVE) == 0);

	/* XXX Try reading etc */

	/* Now, we close the file */
	struct VFS_INODE* file1inode = file1.inode;
	CHECK_OK(vfs_close(&file1));

	/* Now, the backing inode must still exist in the cache... */
	CALCULATE_ICACHE_SIZE(root_inode->fs, icache_size);
	assert(icache_size == 2);
	ii = DQUEUE_HEAD(&root_inode->fs->icache_inuse);
	ii = DQUEUE_NEXT(ii);
	assert(ii->inode == file1inode);
	/* ... but the refcount must be only one (cache) */
	assert(file1inode->refcount == 1);

	/* Try to open a nonexistant file */
	struct VFS_FILE file2;
	CHECK_ERROR(vfs_open(FILE2, NULL, &file1), NO_FILE);

	/* The inode cache mustn't have changed */
	CALCULATE_ICACHE_SIZE(root_inode->fs, icache_size);
	assert(icache_size == 2);

	/* The entry cache must have an extra item... */
	CALCULATE_DCACHE_SIZE(root_inode->fs, dcache_size);
	assert(dcache_size == 2);

	/* ...namely our lookup... */
	di = DQUEUE_HEAD(&root_inode->fs->dcache_inuse);
	di = DQUEUE_NEXT(di);
	assert(di->d_dir_inode == root_inode);
	assert(strcmp(di->d_entry, FILE2) == 0);

	/* ...which must have failed */
	assert(di->d_entry_inode == NULL);
	assert((di->d_flags & DENTRY_FLAG_NEGATIVE) != 0);

	/* Regardless, the root inode's refcount must have not changed */
	assert(root_inode->refcount == 3);

	/*
	 * Part 2: Nested filesystems check.
	 */

	/* Mount a device filesystem on top of our filesystem */
	CHECK_OK(vfs_mount(NULL, "/" DEVFS_MOUNTPOINT, "devfs", NULL));

	/* We must still have 2 inodes in the root filesystem's inode cache... */
	CALCULATE_ICACHE_SIZE(root_inode->fs, icache_size);
	assert(icache_size == 2);

	/* ... with a refcount of 3 (root) and 1 (file1) */
	ii = DQUEUE_HEAD(&root_inode->fs->icache_inuse);
	assert(ii->inode->refcount == 3);
	ii = DQUEUE_NEXT(ii);
	assert(ii->inode->refcount == 1);

	/*
	 * Obtain the new filesystem's root inode; we can't check the inode or entry
 	 * cache without it.
	 */
	struct VFS_FILE file3;
	CHECK_OK(vfs_open(DEVFS_MOUNTPOINT, NULL, &file3));

	/* The filesystem must have a single inode in cache... */
	CALCULATE_ICACHE_SIZE(file3.inode->fs, icache_size);
	assert(icache_size == 1);

	/* ...and it should be our inode, the root one... */
	ii = DQUEUE_HEAD(&file3.inode->fs->icache_inuse);
	assert(ii->inode == file3.inode);
	assert(ii->inode == file3.inode->fs->root_inode);

	/* ...and it must have 3 refs (file3, fs->root_inode, cache) */
	assert(ii->inode->refcount == 3);

	/* The entry cache of the new filesystem must be empty */
	CALCULATE_DCACHE_SIZE(file3.inode->fs, dcache_size);
	assert(dcache_size == 0);

	/* The root filesystem must have 3 entries in the inode cache... */
	CALCULATE_DCACHE_SIZE(root_inode->fs, dcache_size);
	assert(dcache_size == 3);

	/* ...where the first one is our mountpoint... */
	di = DQUEUE_HEAD(&root_inode->fs->dcache_inuse);
	assert(di->d_dir_inode == root_inode);
	assert(di->d_entry_inode == file3.inode);
	assert(strcmp(di->d_entry, DEVFS_MOUNTPOINT) == 0);
	/* ...it must not be a negative entry... */
	assert((di->d_flags & DENTRY_FLAG_NEGATIVE) == 0);
	/* ...but marked permant */
	assert((di->d_flags & DENTRY_FLAG_PERMANENT) != 0);

	/* Check the refcount of the root filesystem; it must not have changed */	
	CALCULATE_ICACHE_SIZE(root_inode->fs, icache_size);
	assert(icache_size == 2);
	ii = DQUEUE_HEAD(&root_inode->fs->icache_inuse);
	assert(ii->inode == root_inode);
	/* Refcount must be 3 (fs->root_inode, root_inode and cache) */
	assert(ii->inode->refcount == 3);
	ii = DQUEUE_NEXT(ii);
	assert(ii->inode == file1inode);
	/* ... but the refcount must be only one (cache) */
	assert(file1inode->refcount == 1);

	return 0;
}

/* vim:set ts=2 sw=2: */
