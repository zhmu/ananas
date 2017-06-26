#ifndef ANANAS_VM_PAGE_H
#define ANANAS_VM_PAGE_H

#include <ananas/types.h>
#include <ananas/list.h>
#include <ananas/lock.h>

struct PAGE;

#define VM_PAGE_FLAG_PRIVATE   (1 << 0)  /* page is private to the process */
#define VM_PAGE_FLAG_READONLY  (1 << 1)  /* page cannot be modified */
#define VM_PAGE_FLAG_COW       (1 << 2)  /* page must be copied on write */
#define VM_PAGE_FLAG_PENDING   (1 << 3)  /* page is pending a read */
#define VM_PAGE_FLAG_LINK      (1 << 4)  /* link to another page */

struct VM_PAGE {
	LIST_FIELDS(struct VM_PAGE);

	mutex_t vp_mtx;

	int vp_flags;
	refcount_t vp_refcount;

	union {
		/* Backing page, if any */
		struct PAGE* vp_page;

		/* Source page */
		struct VM_PAGE* vp_link;
	};

	/* Virtual address mapped to */
	addr_t vp_vaddr;

	/* Backing inode and offset */
	struct VFS_INODE* vp_inode;
	off_t vp_offset;
};

LIST_DEFINE(VM_PAGE_LIST, struct VM_PAGE);

static inline void vmpage_lock(struct VM_PAGE* vmpage)
{
	mutex_lock(&vmpage->vp_mtx);
}

static inline void vmpage_unlock(struct VM_PAGE* vmpage)
{
	mutex_unlock(&vmpage->vp_mtx);
}

void vmpage_ref(struct VM_PAGE* vmpage);
void vmpage_deref(struct VM_PAGE* vmpage);

struct VM_PAGE* vmpage_lookup_locked(vmarea_t* va, struct VFS_INODE* inode, off_t offs);
struct VM_PAGE* vmpage_create_shared(vmarea_t* va, struct VFS_INODE* inode, off_t offs, int flags);
struct VM_PAGE* vmpage_create_private(int flags);
struct PAGE* vmpage_get_page(struct VM_PAGE* vp);

struct VM_PAGE* vmpage_clone(struct VM_PAGE* vp_source);
struct VM_PAGE* vmpage_link(struct VM_PAGE* vp);
void vmpage_copy(struct VM_PAGE* vp_src, struct VM_PAGE* vp_dst);
void vmpage_map(vmspace_t* vs, vmarea_t* va, struct VM_PAGE* vp);
void vmpage_zero(vmspace_t* vs, struct VM_PAGE* vp);

void vmpage_dump(struct VM_PAGE* vp, const char* prefix);

#endif // ANANAS_VM_PAGE_H
