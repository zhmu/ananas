#ifndef ANANAS_VMSPACE_H
#define ANANAS_VMSPACE_H

#include <ananas/types.h>
#include <machine/vmspace.h>
#include <ananas/page.h>

typedef struct VM_AREA vmarea_t;

/* Fault function: handles a page fault for vmspace 'vs', area 'va' and address 'virt' */
typedef errorcode_t (*vmarea_fault_t)(vmspace_t* vs, vmarea_t* va, addr_t virt);

/* Clone function: copies mapping 'vs_src'/'va_src' to 'vs_dst'/'va_dst' */
typedef errorcode_t (*vmarea_clone_t)(vmspace_t* vs_src, vmarea_t* va_src, vmspace_t* vs_dst, vmarea_t* va_dst);

/* Destroy function: cleans up the given mapping's private data */
typedef errorcode_t (*vmarea_destroy_t)(vmspace_t* vs, vmarea_t* va);

/*
 * VM area describes an adjacent mapping though virtual memory.
 */
struct VM_AREA {
	unsigned int		va_flags;		/* flags, combination of VM_FLAG_... */
	addr_t			va_virt;		/* userland address */
	size_t			va_len;			/* length */
	struct page_list	va_pages;		/* backing pages */
	void*			va_privdata;		/* private data */
	vmarea_fault_t	va_fault;		/* fault function */
	vmarea_clone_t	va_clone;		/* clone function */
	vmarea_destroy_t	va_destroy;		/* destroy function */

	DQUEUE_FIELDS(struct VM_AREA);
};

DQUEUE_DEFINE(VM_AREA_LIST, struct VM_AREA);


/*
 * VM space describes a thread's complete overview of memory.
 */
struct VM_SPACE {
	struct VM_AREA_LIST	vs_areas;

	/*
	 * Contains pages allocated to the space that aren't part of a mapping; this
	 * is mainly used to store MD-dependant things like pagetables.
	 */
	struct page_list vs_pages;

	addr_t			vs_next_mapping;	/* address of next mapping */

	MD_VMSPACE_FIELDS
};

errorcode_t vmspace_create(vmspace_t** vs);
void vmspace_cleanup(vmspace_t* vs); /* frees all mappings, but not MD-things */
void vmspace_destroy(vmspace_t* vs);
errorcode_t vmspace_mapto(vmspace_t* vs, addr_t virt, addr_t phys, size_t len /* bytes */, uint32_t flags, vmarea_t** va_out);
errorcode_t vmspace_map(vmspace_t* vs, addr_t phys, size_t len /* bytes */, uint32_t flags, vmarea_t** va_out);
errorcode_t vmspace_area_resize(vmspace_t* vs, vmarea_t* va, size_t new_length /* in bytes */);
errorcode_t vmspace_handle_fault(vmspace_t* vs, addr_t virt, int flags);
errorcode_t vmspace_clone(vmspace_t* vs_source, vmspace_t* vs_dest);
void vmspace_area_free(vmspace_t* vs, vmarea_t* va);

/* MD initialization/cleanup bits */
errorcode_t md_vmspace_init(vmspace_t* vs);
void md_vmspace_destroy(vmspace_t* vs);

#endif /* ANANAS_VMSPACE_H */
