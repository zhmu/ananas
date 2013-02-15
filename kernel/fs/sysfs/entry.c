#include <ananas/types.h>
#include <ananas/lib.h>
#include <ananas/lock.h>
#include <ananas/stat.h>
#include <ananas/mm.h>
#include "entry.h"

static mutex_t sysfs_mutex;
static struct SYSFS_ENTRY* sysfs_root = NULL;
static uint32_t sysfs_cur_inum = SYSFS_ROOTINODE_NUM;

static struct SYSFS_ENTRY*
sysfs_create_entry(const char* name, int mode)
{
	struct SYSFS_ENTRY* entry = kmalloc(strlen(name));
	memset(entry, 0, sizeof(*entry));
	entry->e_inum = sysfs_cur_inum++;
	entry->e_mode = mode;
	entry->e_next = NULL;
	entry->e_children = NULL;
	entry->e_read_fn = NULL;
	entry->e_write_fn = NULL;
	entry->e_refcount = 1; /* ref for caller */
	strcpy(entry->e_name, name);
	return entry;
}

static struct SYSFS_ENTRY*
sysfs_find_entry_from(uint32_t inum, struct SYSFS_ENTRY* root)
{
	if (root->e_inum == inum)
		return root;

	/* Try all entries on this level */
	for (struct SYSFS_ENTRY* entry = root->e_next; entry != NULL; entry = entry->e_next) {
		if (entry->e_inum == inum)
			return entry;
		if (S_ISDIR(entry->e_mode) && entry->e_children != NULL) {
			struct SYSFS_ENTRY* recurse_entry = sysfs_find_entry_from(inum, entry->e_children);
			if (recurse_entry != NULL)
				return recurse_entry;
		}
	}

	if (root->e_children != NULL && root->e_children != root)
		return sysfs_find_entry_from(inum, root->e_children);

	return NULL;
}

struct SYSFS_ENTRY*
sysfs_find_entry(void* fsop)
{
	/*
	 * FIXME This sucks: we're going to do a complete tree walk... and we're
	 * holding the mutex as well!
	 */
	uint32_t inum = *(uint32_t*)fsop;
	mutex_lock(&sysfs_mutex);
	struct SYSFS_ENTRY* entry = sysfs_find_entry_from(inum, sysfs_root);
	mutex_unlock(&sysfs_mutex);
	return entry;
}

static struct SYSFS_ENTRY*
sysfs_make(struct SYSFS_ENTRY* parent, const char* name, int mode)
{
	if (parent == NULL)
		parent = sysfs_root;

	struct SYSFS_ENTRY* entry = sysfs_create_entry(name, mode);

	/* Entry done; hook it to the tree */
	mutex_lock(&sysfs_mutex);
	parent->e_refcount++;
	if (parent->e_children != NULL)
		entry->e_next = parent->e_children;
	parent->e_children = entry;
	mutex_unlock(&sysfs_mutex);
	return entry;
}

struct SYSFS_ENTRY*
sysfs_mkdir(struct SYSFS_ENTRY* parent, const char* name, int mode)
{
	KASSERT((mode & S_IFMT) == 0, "mode %x invalid", mode);
	return sysfs_make(parent, name, S_IFDIR | mode);
}

struct SYSFS_ENTRY*
sysfs_mkreg(struct SYSFS_ENTRY* parent, const char* name, int mode)
{
	KASSERT((mode & S_IFMT) == 0, "mode %x invalid", mode);
	return sysfs_make(parent, name, S_IFREG | mode);
}

void
sysfs_init_structs()
{
	mutex_init(&sysfs_mutex, "sysfs");
	sysfs_root = sysfs_create_entry(".", S_IFDIR | 0555);
	sysfs_root->e_children = sysfs_root; /* XXX kludge to simplify lookups */
	sysfs_root->e_refcount++; /* always keep one ref, kernel has it */
}

/* vim:set ts=2 sw=2: */
