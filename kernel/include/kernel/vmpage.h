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
#define VM_PAGE_FLAG_PROMOTED  (1 << 5)  /* XXX this is for debugging only */

class VMArea;
class VMSpace;
struct INode;

struct VMPage : util::List<VMPage>::NodePtr {
	VMPage(VMArea* va, INode* inode, off_t offset, int flags)
		: vp_vmarea(va), vp_inode(inode), vp_offset(offset), vp_flags(flags), vp_page(nullptr)
	{
		vp_refcount = 1; // instantiator
	}
	~VMPage();

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
	addr_t vp_vaddr = 0;

	/* Backing inode and offset */
	INode* vp_inode;
	off_t vp_offset;

	void Ref();
	void Deref();
	Page* GetPage();
	VMPage& Resolve();

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
	void CopyExtended(VMPage& vp_dst, size_t len);

	void Copy(VMPage& vp_dst)
	{
	  CopyExtended(vp_dst, PAGE_SIZE);
	}

	void Lock() {
		vp_mtx.Lock();
	}

	void Unlock() {
		vp_mtx.Unlock();
	}

	void AssertLocked();

	void AssertNotLinked();
	VMPage& Link(VMArea& va_dest);

	void Map(VMSpace& vs, VMArea& va);
	void Zero(VMSpace& vs);
	void Dump(const char* prefix) const;

private:
	Mutex vp_mtx{"vmpage"};
};

typedef util::List<VMPage> VMPageList;

namespace vmpage {

VMPage& Allocate(VMArea& va, INode* inode, off_t offset, int flags);

} // namespace vmspace

VMPage* vmpage_lookup_locked(VMArea& va, INode& inode, off_t offs);
VMPage& vmpage_create_shared(INode& inode, off_t offs, int flags);

VMPage& vmpage_clone(VMSpace* vs_source, VMSpace& vs_dest, VMArea& va_source, VMArea& va_dest, VMPage& vp);

#endif // ANANAS_VM_PAGE_H
