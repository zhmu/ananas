#ifndef ANANAS_VMSPACE_H
#define ANANAS_VMSPACE_H

#include <ananas/types.h>
#include <ananas/util/list.h>
#include "kernel/page.h"
#include "kernel/vmpage.h"
#include "kernel-md/vmspace.h"

struct DEntry;

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
	unsigned int		va_flags;		/* flags, combination of VM_FLAG_... */
	addr_t			va_virt;		/* userland address */
	size_t			va_len;			/* length */
	VMPageList		va_pages;		/* backing pages */
	/* dentry-specific mapping fields */
	DEntry* 		va_dentry;		/* backing dentry, if any */
	off_t			va_doffset;		/* dentry offset */
	size_t			va_dlength;		/* dentry length */
};
typedef util::List<VMArea> VMAreaList;

/*
 * VM space describes a thread's complete overview of memory.
 */
struct VMSpace {
	Mutex vs_mutex; /* protects all fields and sub-areas */

	VMAreaList vs_areas;

	/*
	 * Contains pages allocated to the space that aren't part of a mapping; this
	 * is mainly used to store MD-dependant things like pagetables.
	 */
	PageList		vs_pages;

	addr_t			vs_next_mapping;	/* address of next mapping */

	MD_VMSPACE_FIELDS
};

#define VMSPACE_CLONE_EXEC 1

addr_t vmspace_determine_va(VMSpace& vs, size_t len);
errorcode_t vmspace_create(VMSpace*& vs);
void vmspace_cleanup(VMSpace& vs); /* frees all mappings, but not MD-things */
void vmspace_destroy(VMSpace& vs);
errorcode_t vmspace_mapto(VMSpace& vs, addr_t virt, size_t len /* bytes */, uint32_t flags, VMArea*& va_out);
errorcode_t vmspace_mapto_dentry(VMSpace& vs, addr_t virt, size_t vlength, DEntry& dentry, off_t doffset, size_t dlength, int flags, VMArea*& va_out);
errorcode_t vmspace_map(VMSpace& vs, size_t len /* bytes */, uint32_t flags, VMArea*& va_out);
errorcode_t vmspace_area_resize(VMSpace& vs, VMArea& va, size_t new_length /* in bytes */);
errorcode_t vmspace_handle_fault(VMSpace& vs, addr_t virt, int flags);
errorcode_t vmspace_clone(VMSpace& vs_source, VMSpace& vs_dest, int flags);
void vmspace_area_free(VMSpace& vs, VMArea& va);
void vmspace_dump(VMSpace& vs);

/* MD initialization/cleanup bits */
errorcode_t md_vmspace_init(VMSpace& vs);
void md_vmspace_destroy(VMSpace& vs);

static inline void vmspace_lock(VMSpace& vs)
{
	mutex_lock(vs.vs_mutex);
}

static inline void vmspace_unlock(VMSpace& vs)
{
	mutex_unlock(vs.vs_mutex);
}

static inline void vmspace_assert_locked(VMSpace& vs)
{
	mutex_assert(vs.vs_mutex, MTX_LOCKED);
}

#endif /* ANANAS_VMSPACE_H */
