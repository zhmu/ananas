#ifndef ANANAS_VMSPACE_H
#define ANANAS_VMSPACE_H

#include <ananas/types.h>
#include "kernel/list.h"
#include "kernel/page.h"
#include "kernel/vmpage.h"
#include "kernel-md/vmspace.h"

/*
 * VM area describes an adjacent mapping though virtual memory. It can be
  backed by an inode in the following way:
 *
 *                         +------+
 *              va_doffset |      |
 *       +-------+       \ |      |
 *       |       |        \|      |
  *      |d_vskip|>--------+......| ^
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
struct VM_AREA {
	unsigned int		va_flags;		/* flags, combination of VM_FLAG_... */
	addr_t			va_virt;		/* userland address */
	size_t			va_len;			/* length */
	struct VM_PAGE_LIST	va_pages;		/* backing pages */
	/* dentry-specific mapping fields */
	struct DENTRY* 		va_dentry;		/* backing dentry, if any */
	off_t			va_dvskip;		/* number of initial bytes to skip */
	off_t			va_doffset;		/* dentry offset */
	size_t			va_dlength;		/* dentry length */

	LIST_FIELDS(struct VM_AREA);
};

LIST_DEFINE(VM_AREA_LIST, struct VM_AREA);

/*
 * VM space describes a thread's complete overview of memory.
 */
struct VM_SPACE {
	mutex_t vs_mutex; /* protects all fields and sub-areas */

	struct VM_AREA_LIST	vs_areas;

	/*
	 * Contains pages allocated to the space that aren't part of a mapping; this
	 * is mainly used to store MD-dependant things like pagetables.
	 */
	struct page_list vs_pages;

	addr_t			vs_next_mapping;	/* address of next mapping */

	MD_VMSPACE_FIELDS
};

#define VMSPACE_CLONE_EXEC 1

errorcode_t vmspace_create(vmspace_t** vs);
void vmspace_cleanup(vmspace_t* vs); /* frees all mappings, but not MD-things */
void vmspace_destroy(vmspace_t* vs);
errorcode_t vmspace_mapto(vmspace_t* vs, addr_t virt, size_t len /* bytes */, uint32_t flags, vmarea_t** va_out);
errorcode_t vmspace_mapto_dentry(vmspace_t* vs, addr_t virt, off_t vskip, size_t vlength, struct DENTRY* dentry, off_t doffset, size_t dlength, int flags, vmarea_t** va_out);
errorcode_t vmspace_map(vmspace_t* vs, size_t len /* bytes */, uint32_t flags, vmarea_t** va_out);
errorcode_t vmspace_area_resize(vmspace_t* vs, vmarea_t* va, size_t new_length /* in bytes */);
errorcode_t vmspace_handle_fault(vmspace_t* vs, addr_t virt, int flags);
errorcode_t vmspace_clone(vmspace_t* vs_source, vmspace_t* vs_dest, int flags);
void vmspace_area_free(vmspace_t* vs, vmarea_t* va);
void vmspace_dump(vmspace_t* vs);

/* MD initialization/cleanup bits */
errorcode_t md_vmspace_init(vmspace_t* vs);
void md_vmspace_destroy(vmspace_t* vs);

static inline void vmspace_lock(vmspace_t* vs)
{
	mutex_lock(&vs->vs_mutex);
}

static inline void vmspace_unlock(vmspace_t* vs)
{
	mutex_unlock(&vs->vs_mutex);
}

static inline void vmspace_assert_locked(vmspace_t* vs)
{
	mutex_assert(&vs->vs_mutex, MTX_LOCKED);
}

#endif /* ANANAS_VMSPACE_H */
