#ifndef ANANAS_VMSPACE_H
#define ANANAS_VMSPACE_H

#include <ananas/types.h>
#include <ananas/util/list.h>
#include "kernel/page.h"
#include "kernel/vmpage.h"
#include "kernel-md/vmspace.h"

struct DEntry;
class Result;

/*
 * VM area describes an adjacent mapping though virtual memory. It can be
  backed by an inode in the following way:
 *
 *                         +------+
 *              va_doffset |      |
 *       +-------+\      \ |      |
 *       |       | \      \|      |
  *      |       |  -------+......| ^
 *       |       |         | file | | va_dlength
 *       +-------+         |      | |
 *       |       |<--------+......| v
 *       | page2 |         |      |
 *       |       |         |      |
 *       +-------+         |      |
 *                         +------+
 *
 * Note that we'll only fault one page at a time.
 *
 */
struct VMArea : util::List<VMArea>::NodePtr {
	unsigned int		va_flags = 0;		/* flags, combination of VM_FLAG_... */
	addr_t			va_virt = 0;		/* userland address */
	size_t			va_len = 0;		/* length */
	VMPageList		va_pages;		/* backing pages */
	/* dentry-specific mapping fields */
	DEntry* 		va_dentry = nullptr;	/* backing dentry, if any */
	off_t			va_doffset = 0;		/* dentry offset */
	size_t			va_dlength = 0;		/* dentry length */
};
typedef util::List<VMArea> VMAreaList;

/*
 * VM space describes a thread's complete overview of memory.
 */
struct VMSpace {

	void Lock()
	{
		vs_mutex.Lock();
	}

	void Unlock()
	{
		vs_mutex.Unlock();
	}

	void AssertLocked()
	{
		vs_mutex.AssertLocked();
	}

	VMAreaList vs_areas;

	/*
	 * Contains pages allocated to the space that aren't part of a mapping; this
	 * is mainly used to store MD-dependant things like pagetables.
	 */
	PageList		vs_pages;

	addr_t			vs_next_mapping;	/* address of next mapping */

	MD_VMSPACE_FIELDS

	Result MapTo(addr_t virt, size_t len /* bytes */, uint32_t flags, VMArea*& va_out);
	Result MapToDentry(addr_t virt, size_t vlength, DEntry& dentry, off_t doffset, size_t dlength, int flags, VMArea*& va_out);
	Result Map(size_t len /* bytes */, uint32_t flags, VMArea*& va_out);
	Result Clone(VMSpace& vs_dest, int flags);
	void Dump();

	// XXX Should not be public
	void FreeArea(VMArea& va);

private:
	Mutex vs_mutex{"vmspace"}; /* protects all fields and sub-areas */
};

#define VMSPACE_CLONE_EXEC 1

addr_t vmspace_determine_va(VMSpace& vs, size_t len);
Result vmspace_create(VMSpace*& vs);
void vmspace_destroy(VMSpace& vs);

Result vmspace_handle_fault(VMSpace& vs, addr_t virt, int flags);

#endif /* ANANAS_VMSPACE_H */
