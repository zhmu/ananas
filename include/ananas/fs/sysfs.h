#ifndef __ANANAS_SYSFS_H__ 
#define __ANANAS_SYSFS_H__

#include <ananas/types.h>

struct SYSFS_ENTRY;
struct VFS_FILE;

/*
 * Callback for reading an entry; buffer is one page that is free to use and offset is the offset to read, with *len
 * the amount requested. Upon success, set start to the data (within buffer) to return, spanning *len bytes.
 */
typedef errorcode_t (*sysfs_read_entry_t)(struct SYSFS_ENTRY* entry, void* buffer, char** start, off_t offset, size_t* len);
typedef errorcode_t (*sysfs_write_entry_t)(struct SYSFS_ENTRY* entry, struct VFS_FILE* file, const void* buffer, size_t* len);

/*
 * All sysfs entries are kept using linked lists; one for children and
 * one for the same level.
 *
 * The entire sysfs entry tree is protected using a single mutex; the idea is
 * that modifications should be fairly rare.
 *
 * Note that all of this isn't the speediest approach but we'll deal with that
 * as necessary.
 */
struct SYSFS_ENTRY {
	refcount_t e_refcount;
	uint32_t e_inum;
	int e_mode;
	/* Next pointers on this level */
	struct SYSFS_ENTRY* e_next;
	/* Pointer to children */
	struct SYSFS_ENTRY* e_children;
	/* Read/write operations */
	sysfs_read_entry_t e_read_fn;
	sysfs_write_entry_t e_write_fn;
	char e_name[1];
};


/* entry.c */
struct SYSFS_ENTRY* sysfs_mkdir(struct SYSFS_ENTRY* parent, const char* name, int mode);
struct SYSFS_ENTRY* sysfs_mkreg(struct SYSFS_ENTRY* parent, const char* name, int mode);

/* helper.c */
errorcode_t sysfs_read_string(char* string, char** start, off_t offset, size_t* len);

#endif /* __ANANAS_SYSFS_H__ */
