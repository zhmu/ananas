#include <ananas/types.h>
#include <machine/param.h> /* for THREAD_INITIAL_MAPPING_ADDR */
#include <machine/vm.h> /* for md_{,un}map_pages() */
#include <ananas/kmem.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/error.h>
#include <ananas/vfs/dentry.h>
#include <ananas/trace.h>
#include <ananas/vm.h>
#include <ananas/vmpage.h>
#include <ananas/vmspace.h>

TRACE_SETUP;

#define BYTES_TO_PAGES(len) ((len + PAGE_SIZE - 1) / PAGE_SIZE)

errorcode_t
vmspace_create(vmspace_t** vmspace)
{
	auto vs = new vmspace_t;
	memset(vs, 0, sizeof(*vs));
	LIST_INIT(&vs->vs_pages);
	vs->vs_next_mapping = THREAD_INITIAL_MAPPING_ADDR;

	errorcode_t err = md_vmspace_init(vs);
	ANANAS_ERROR_RETURN(err);

	mutex_init(&vs->vs_mutex, "vmspace");
	*vmspace = vs;
	return err;
}

void
vmspace_cleanup(vmspace_t* vs)
{
	/* Cleanup only removes all mapped areas */
	while(!LIST_EMPTY(&vs->vs_areas)) {
		vmarea_t* va = LIST_HEAD(&vs->vs_areas);
		vmspace_area_free(vs, va);
	}
}

void
vmspace_destroy(vmspace_t* vs)
{
	/* Ensure all mapped areas are gone (can't hurt if this is already done) */
	vmspace_cleanup(vs);

	/* Remove the vmspace-specific mappings - these are generally MD */
	LIST_FOREACH_SAFE(&vs->vs_pages, p, struct PAGE) {
		/* XXX should we unmap the page here? the vmspace shouldn't be active... */
		page_free(p);
	}
	md_vmspace_destroy(vs);
	kfree(vs);
}

static int
vmspace_is_inuse(vmspace_t* vs, addr_t virt, size_t len)
{
	LIST_FOREACH(&vs->vs_areas, va, vmarea_t) {
		if ((virt >= va->va_virt && (virt + len) <= (va->va_virt + va->va_len)) ||
				(va->va_virt >= virt && (va->va_virt + va->va_len) <= (virt + len)))
			return 1;
	}

	return 0;
}

errorcode_t
vmspace_mapto(vmspace_t* vs, addr_t virt, addr_t phys, size_t len /* bytes */, uint32_t flags, vmarea_t** va_out)
{
	/* First, ensure the range isn't used and the length is sane */
	if(vmspace_is_inuse(vs, virt, len))
		return ANANAS_ERROR(NO_SPACE);
	if (len == 0)
		return ANANAS_ERROR(BAD_LENGTH);

	auto va = new vmarea_t;
	memset(va, 0, sizeof(*va));

	/*
	 * XXX We should ask the VM for some kind of reservation of the
	 * THREAD_MAP_ALLOC flag is set; now we'll just assume that the
	 * memory is there...
	 */
	LIST_INIT(&va->va_pages);
	va->va_virt = virt;
	va->va_len = len;
	va->va_flags = flags;
	LIST_APPEND(&vs->vs_areas, va);
	TRACE(VM, INFO, "vmspace_mapto(): vs=%p, va=%p, phys=%p, virt=%p, flags=0x%x", vs, va, phys, virt, flags);
	*va_out = va;

	/* Provide a mapping for the pages */
	md_map_pages(vs, va->va_virt, phys, BYTES_TO_PAGES(len), (flags & VM_FLAG_FAULT) ? 0 : flags);
	return ananas_success();
}

errorcode_t
vmspace_mapto_dentry(vmspace_t* vs, addr_t virt, off_t vskip, size_t vlength, struct DENTRY* dentry, off_t doffset, size_t dlength, int flags, vmarea_t** va_out)
{
	errorcode_t err = vmspace_mapto(vs, virt, (addr_t)NULL, vlength, flags | VM_FLAG_FAULT, va_out);
	ANANAS_ERROR_RETURN(err);

	dentry_ref(dentry);
	(*va_out)->va_dentry = dentry;
	(*va_out)->va_dvskip = vskip;
	(*va_out)->va_doffset = doffset;
	(*va_out)->va_dlength = dlength;
	return ananas_success();
}

errorcode_t
vmspace_map(vmspace_t* vs, addr_t phys, size_t len /* bytes */, uint32_t flags, vmarea_t** va_out)
{
	/*
	 * Locate a new address to map to; we currently never re-use old addresses.
	 */
	addr_t virt = vs->vs_next_mapping;
	vs->vs_next_mapping += len;
	if ((vs->vs_next_mapping & (PAGE_SIZE - 1)) > 0)
		vs->vs_next_mapping += PAGE_SIZE - (vs->vs_next_mapping & (PAGE_SIZE - 1));
	return vmspace_mapto(vs, virt, phys, len, flags, va_out);
}

/*
 * vmspace_clone() is used for two scenarios:
 *
 * (1) fork() uses it to copy the parent's vmspace to a new child
 * (2) exec() uses it to fill the current vmspace with the new one
 *
 * In the first case, we just need to make things as identical as possible; yet
 * for (2), the destination vmspace will not have MD-specific data as no
 * threads were ever created in it.
 *
 * As the caller knows this information, we'll let the decision rest in the
 * hands of the caller - if we are cloning for exec(), we'll clone MD-fields.
 */
static inline int
vmspace_clone_area_must_free(vmarea_t* va, int flags)
{
	/* Scenario (2) does not free MD-specific parts */
	if ((flags & VMSPACE_CLONE_EXEC) && (va->va_flags & VM_FLAG_MD))
		return 0;
	return (va->va_flags & VM_FLAG_NO_CLONE) == 0;
}

static inline int
vmspace_clone_area_must_copy(vmarea_t* va, int flags)
{
	/* Scenario (2) does copy MD-specific parts */
	if ((flags & VMSPACE_CLONE_EXEC) && (va->va_flags & VM_FLAG_MD))
		return 1;
	return (va->va_flags & VM_FLAG_NO_CLONE) == 0;
}

errorcode_t
vmspace_clone(vmspace_t* vs_source, vmspace_t* vs_dest, int flags)
{
	TRACE(VM, INFO, "vmspace_clone(): source=%p dest=%p flags=%x", vs_source, vs_dest, flags);

	/*
	 * First, clean up the destination area's mappings - this ensures we'll
	 * overwrite them with our own. Note that we'll leave private mappings alone.
	 */
	LIST_FOREACH_SAFE(&vs_dest->vs_areas, va, vmarea_t) {
		if (!vmspace_clone_area_must_free(va, flags))
			continue;
		vmspace_area_free(vs_dest, va);
	}

	/* Now copy everything over that isn't private */
	LIST_FOREACH(&vs_source->vs_areas, va_src, vmarea_t) {
		if (!vmspace_clone_area_must_copy(va_src, flags))
			continue;

		vmarea_t* va_dst;
		errorcode_t err = vmspace_mapto(vs_dest, va_src->va_virt, 0, va_src->va_len, VM_FLAG_FAULT | va_src->va_flags, &va_dst);
		ANANAS_ERROR_RETURN(err);
		if (va_src->va_dentry != nullptr) {
			// Backed by an inode; copy the necessary fields over
			va_dst->va_doffset = va_src->va_doffset;
			va_dst->va_dvskip = va_src->va_dvskip;
			va_dst->va_dlength = va_src->va_dlength;
			va_dst->va_dentry = va_src->va_dentry;
			dentry_ref(va_dst->va_dentry);
		}

		// Copy the area page-wise
		LIST_FOREACH(&va_src->va_pages, vp, struct VM_PAGE) {
			KASSERT(vmpage_get_page(vp)->p_order == 0, "unexpected %d order page here", vmpage_get_page(vp)->p_order);

			// Create a clone of the data; it is up to the vmpage how to do this (it may go for COW)
			struct VM_PAGE* new_vp = vmpage_clone(vp);
			LIST_APPEND(&va_dst->va_pages, new_vp);

			// Map the page into the cloned vmspace
			vmpage_map(vs_dest, va_dst, new_vp);
		}
	}

	/*
	 * See where the next mapping can be placed; we should use something more
	 * clever than vs_next_mapping someday but this will have to suffice for
	 * now.
	 */
	vs_dest->vs_next_mapping = 0;
	LIST_FOREACH(&vs_dest->vs_areas, va, vmarea_t) {
		addr_t next = va->va_virt + va->va_len;
		if (vs_dest->vs_next_mapping < next)
			vs_dest->vs_next_mapping = next;
	}

	return ananas_success();
}

void
vmspace_area_free(vmspace_t* vs, vmarea_t* va)
{
	LIST_REMOVE(&vs->vs_areas, va);

	/* Free any backing dentry, if we have one */
	if (va->va_dentry != nullptr)
		dentry_deref(va->va_dentry);

	/* If the pages were allocated, we need to free them one by one */
	LIST_FOREACH_SAFE(&va->va_pages, vp, struct VM_PAGE) {
		vmpage_deref(vp);
	}
	kfree(va);
}

void
vmspace_dump(vmspace_t* vs)
{
	LIST_FOREACH(&vs->vs_areas, va, vmarea_t) {
		kprintf("  area %p: %p..%p flags %c%c%c%c%c%c%c%c\n",
		 va, va->va_virt, va->va_virt + va->va_len - 1,
		 (va->va_flags & VM_FLAG_READ) ? 'r' : '.',
		 (va->va_flags & VM_FLAG_WRITE) ? 'w' : '.',
		 (va->va_flags & VM_FLAG_EXECUTE) ? 'x' : '.',
		 (va->va_flags & VM_FLAG_KERNEL) ? 'k' : '.',
		 (va->va_flags & VM_FLAG_USER) ? 'u' : '.',
		 (va->va_flags & VM_FLAG_PRIVATE) ? 'p' : '.',
		 (va->va_flags & VM_FLAG_NO_CLONE) ? 'n' : '.',
		 (va->va_flags & VM_FLAG_MD) ? 'm' : '.');
		kprintf("    pages:\n");
		LIST_FOREACH(&va->va_pages, vp, struct VM_PAGE) {
			vmpage_dump(vp, "      ");
		}
	}
}

/* vim:set ts=2 sw=2: */
