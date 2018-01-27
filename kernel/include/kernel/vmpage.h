#ifndef ANANAS_VM_PAGE_H
#define ANANAS_VM_PAGE_H

#include <ananas/types.h>
#include <ananas/util/list.h>
#include <machine/param.h>
#include "kernel/lock.h"

struct Page;

#define VM_PAGE_FLAG_PRIVATE   (1 << 0)  /* page is private to the process */
#define VM_PAGE_FLAG_READONLY  (1 << 1)  /* page cannot be modified */
#define VM_PAGE_FLAG_COW       (1 << 2)  /* page must be copied on write */
#define VM_PAGE_FLAG_PENDING   (1 << 3)  /* page is pending a read */
#define VM_PAGE_FLAG_LINK      (1 << 4)  /* link to another page */

class VMArea;
class VMSpace;
struct INode;

struct VMPage : util::List<VMPage>::NodePtr {
	Mutex vp_mtx;
	VMArea* vp_vmarea;

	int vp_flags;
	refcount_t vp_refcount;

	union {
		/* Backing page, if any */
		Page* vp_page;

		/* Source page */
		VMPage* vp_link;
	};

	/* Virtual address mapped to */
	addr_t vp_vaddr;

	/* Backing inode and offset */
	INode* vp_inode;
	off_t vp_offset;
};

typedef util::List<VMPage> VMPageList;

#define vmpage_lock(vp) \
	mutex_lock((vp).vp_mtx)

#define vmpage_unlock(vp) \
	mutex_unlock((vp).vp_mtx)

#define vmpage_assert_locked(vp) \
	mutex_assert((vp).vp_mtx, MTX_LOCKED)

void vmpage_ref(VMPage& vmpage);
void vmpage_deref(VMPage& vmpage);

VMPage* vmpage_lookup_locked(VMArea& va, INode& inode, off_t offs);
VMPage* vmpage_lookup_vaddr_locked(VMArea& va, addr_t vaddr);
VMPage& vmpage_create_shared(INode& inode, off_t offs, int flags);
VMPage& vmpage_create_private(VMArea* va, int flags);
Page* vmpage_get_page(VMPage& vp);

VMPage& vmpage_clone(VMSpace& vs, VMArea& va_source, VMArea& va_dest, VMPage& vp);
VMPage& vmpage_link(VMArea& va, VMPage& vp);
void vmpage_map(VMSpace& vs, VMArea& va, VMPage& vp);
void vmpage_zero(VMSpace& vs, VMPage& vp);
VMPage& vmpage_promote(VMSpace& vs, VMArea& va, VMPage& vp);

void vmpage_dump(const VMPage& vp, const char* prefix);

/*
 * Copies a (piece of) vp_src to vp_dst:
 *
 *        len
 *         /
 * +-------+------+
 * |XXXXXXX|??????|
 * +-------+------+
 *         |
 *         v
 * +-------+---+
 * |XXXXXXX|000|
 * +-------+---+
 *         ^    \
 *        len    PAGE_SIZE
 *
 *
 * X = bytes to be copied, 0 = bytes set to zero, ? = don't care - note that
 * thus the vp_dst page is always completely filled.
 */
void vmpage_copy_extended(VMPage& vp_src, VMPage& vp_dst, size_t len);

static inline void
vmpage_copy(VMPage& vp_src, VMPage& vp_dst)
{
  vmpage_copy_extended(vp_src, vp_dst, PAGE_SIZE);
}

#endif // ANANAS_VM_PAGE_H
