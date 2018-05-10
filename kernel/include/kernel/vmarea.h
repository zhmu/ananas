#ifndef ANANAS_VMAREA_H
#define ANANAS_VMAREA_H

#include <ananas/types.h>
#include <ananas/util/list.h>

struct DEntry;
struct VMPage;
struct VMSpace;

/*
 * VM area describes an adjacent mapping though virtual memory. It can be
 * backed by an inode in the following way:
 *
 *                         +------+
 *              va_doffset |      |
 *       +-------+\      \ |      |
 *       |       | \      \|      |
 *       |       |  -------+......| ^
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
	VMArea(VMSpace& vs, addr_t virt, size_t len, int flags)
	 : va_vs(vs), va_virt(virt), va_len(len), va_flags(flags)
	{
	}

	VMSpace& va_vs;

	VMPage* LookupVAddrAndLock(addr_t vaddr);
	VMPage& AllocatePrivatePage(addr_t vaddr, int flags);
	VMPage& PromotePage(VMPage& vp);

	unsigned int		va_flags = 0;		/* flags, combination of VM_FLAG_... */
	addr_t			va_virt = 0;		/* userland address */
	size_t			va_len = 0;		/* length */
	util::List<VMPage>	va_pages;		/* backing pages */
	/* dentry-specific mapping fields */
	DEntry* 		va_dentry = nullptr;	/* backing dentry, if any */
	off_t			va_doffset = 0;		/* dentry offset */
	size_t			va_dlength = 0;		/* dentry length */
};
typedef util::List<VMArea> VMAreaList;

#endif /* ANANAS_VMAREA_H */
