#ifndef ANANAS_VM_PAGE_H
#define ANANAS_VM_PAGE_H

#include <ananas/types.h>
#include <machine/param.h>
#include "kernel/list.h"
#include "kernel/lock.h"

struct PAGE;

#define VM_PAGE_FLAG_PRIVATE   (1 << 0)  /* page is private to the process */
#define VM_PAGE_FLAG_READONLY  (1 << 1)  /* page cannot be modified */
#define VM_PAGE_FLAG_COW       (1 << 2)  /* page must be copied on write */
#define VM_PAGE_FLAG_PENDING   (1 << 3)  /* page is pending a read */
#define VM_PAGE_FLAG_LINK      (1 << 4)  /* link to another page */

struct VM_PAGE {
	LIST_FIELDS(struct VM_PAGE);

	mutex_t vp_mtx;
	vmarea_t* vp_vmarea;

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

#define vmpage_lock(vp) \
	mutex_lock(&(vp)->vp_mtx)

#define vmpage_unlock(vp) \
	mutex_unlock(&(vp)->vp_mtx)

#define vmpage_assert_locked(vp) \
	mutex_assert(&(vp)->vp_mtx, MTX_LOCKED)

void vmpage_ref(struct VM_PAGE* vmpage);
void vmpage_deref(struct VM_PAGE* vmpage);

struct VM_PAGE* vmpage_lookup_locked(vmarea_t* va, struct VFS_INODE* inode, off_t offs);
struct VM_PAGE* vmpage_lookup_vaddr_locked(vmarea_t* va, addr_t vaddr);
struct VM_PAGE* vmpage_create_shared(struct VFS_INODE* inode, off_t offs, int flags);
struct VM_PAGE* vmpage_create_private(vmarea_t* va, int flags);
struct PAGE* vmpage_get_page(struct VM_PAGE* vp);

struct VM_PAGE* vmpage_clone(vmspace_t* vs, vmarea_t* va_source, vmarea_t* va_dest, struct VM_PAGE* vp);
struct VM_PAGE* vmpage_link(vmarea_t* va, struct VM_PAGE* vp);
void vmpage_map(vmspace_t* vs, vmarea_t* va, struct VM_PAGE* vp);
void vmpage_zero(vmspace_t* vs, struct VM_PAGE* vp);
struct VM_PAGE* vmpage_promote(vmspace_t* vs, vmarea_t* va, struct VM_PAGE* vp);

void vmpage_dump(struct VM_PAGE* vp, const char* prefix);

/*
 * Copies a (piece of) vp_src to vp_dst:
 *
 *      src_off   src_off + len
 *      v        /
 * +----+-------+------+
 * |????|XXXXXXX|??????|
 * +----+-------+------+
 *         |
 *         v
 * +-------+-------+---+
 * |0000000|XXXXXXX|000+
 * +-------+-------+---+
 *         ^        \
 *     dst_off       dst_off + len
 *
 *
 * X = bytes to be copied, 0 = bytes set to zero, ? = don't care
 */
void vmpage_copy_extended(struct VM_PAGE* vp_src, struct VM_PAGE* vp_dst, size_t len, size_t src_off, size_t dst_off);

static inline void
vmpage_copy(struct VM_PAGE* vp_src, struct VM_PAGE* vp_dst)
{
  vmpage_copy_extended(vp_src, vp_dst, PAGE_SIZE, 0, 0);
}

#endif // ANANAS_VM_PAGE_H
