#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/trace.h"
#include "kernel/vmpage.h"
#include "kernel/vmspace.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vfs/types.h"
#include "kernel/vm.h"
#include "kernel-md/param.h" // for THREAD_INITIAL_MAPPING_ADDR
#include "kernel-md/vm.h" // for md_{,un}map_pages()

TRACE_SETUP;

namespace
{

size_t BytesToPages(size_t len)
{
	return (len + PAGE_SIZE - 1) / PAGE_SIZE;
}

addr_t RoundUp(addr_t addr)
{
	if ((addr & (PAGE_SIZE - 1)) == 0)
		return addr;

	return (addr | (PAGE_SIZE - 1)) + 1;
}

} // unnamed namespace

addr_t
vmspace_determine_va(VMSpace& vs, size_t len)
{
	/*
	 * XXX This is a bit of a kludge - besides, we currently never re-use old
	 * addresses which may get ugly.
	 */
	addr_t virt = vs.vs_next_mapping;
	vs.vs_next_mapping = RoundUp(vs.vs_next_mapping + len);
	return virt;
}

errorcode_t
vmspace_create(VMSpace*& vmspace)
{
	auto vs = new VMSpace;
	memset(vs, 0, sizeof(*vs));
	vs->vs_next_mapping = THREAD_INITIAL_MAPPING_ADDR;

	errorcode_t err = md_vmspace_init(*vs);
	ANANAS_ERROR_RETURN(err);

	mutex_init(&vs->vs_mutex, "vmspace");
	vmspace = vs;
	return err;
}

void
vmspace_cleanup(VMSpace& vs)
{
	/* Cleanup only removes all mapped areas */
	while(!vs.vs_areas.empty()) {
		VMArea& va = vs.vs_areas.front();
		vmspace_area_free(vs, va);
	}
}

void
vmspace_destroy(VMSpace& vs)
{
	/* Ensure all mapped areas are gone (can't hurt if this is already done) */
	vmspace_cleanup(vs);

	/* Remove the vmspace-specific mappings - these are generally MD */
	for(auto it = vs.vs_pages.begin(); it != vs.vs_pages.end(); /* nothing */) {
		auto& p = *it; ++it;
		/* XXX should we unmap the page here? the vmspace shouldn't be active... */
		page_free(p);
	}
	md_vmspace_destroy(vs);
	delete &vs;
}

static bool
vmspace_free_range(VMSpace& vs, addr_t virt, size_t len)
{
	for(auto& va: vs.vs_areas) {
		/*
		 * To keep things simple, we will only break up mappings if they occur
		 * completely within a single - this avoids complicated merging logic.
		 */
		if (virt >= va.va_virt + va.va_len || virt + len <= va.va_virt)
			continue; // not within our area, skip

		// See if this range extends before or beyond our va - if that is the case, we
		// will reject it
		if (virt < va.va_virt || virt + len > va.va_virt + va.va_len)
			return false;

		// Okay, we can alter this va to exclude our mapping. If it matches 1-to-1, just throw it away
		if (virt == va.va_virt && len == va.va_len) {
			vmspace_area_free(vs, va);
			return true;
		}

		// Not a full match; maybe at the start?
		if (virt == va.va_virt) {
			// Shift the entire range, but ensure we'll honor page-boundaries
			va.va_virt = RoundUp(va.va_virt + len);
			va.va_len = RoundUp(va.va_len - len);
			if (va.va_dentry != nullptr) {
				// Backed by an inode; correct the offsets
				va.va_doffset += len;
				if (va.va_dlength >= len)
					va.va_dlength -= len;
				else
					va.va_dlength = 0;
				KASSERT((va.va_doffset & (PAGE_SIZE - 1)) == 0, "doffset %x not page-aligned", (int)va.va_doffset);
			}

			// Make sure we will have space for our next mapping XXX
			addr_t next_addr = va.va_virt + va.va_len;
			if (vs.vs_next_mapping < next_addr)
				vs.vs_next_mapping = next_addr;
			return true;
		}

		// XXX we need to cover the other use cases here as well (range at end, range in the middle)
		panic("implement me! virt=%p..%p, va=%p..%p", virt, virt+len, va.va_virt, va.va_virt+va.va_len);
	}

	// No ranges match; this is okay
	return true;
}

errorcode_t
vmspace_mapto(VMSpace& vs, addr_t virt, size_t len /* bytes */, uint32_t flags, VMArea*& va_out)
{
	if (len == 0)
		return ANANAS_ERROR(BAD_LENGTH);

	// If the virtual address space is already in use, we need to break it up
	if (!vmspace_free_range(vs, virt, len))
		return ANANAS_ERROR(NO_SPACE);

	auto va = new VMArea;
	memset(va, 0, sizeof(*va));

	/*
	 * XXX We should ask the VM for some kind of reservation if the
	 * THREAD_MAP_ALLOC flag is set; now we'll just assume that the
	 * memory is there...
	 */
	va->va_pages.clear();
	va->va_virt = virt;
	va->va_len = len;
	va->va_flags = flags;
	vs.vs_areas.push_back(*va);
	TRACE(VM, INFO, "vmspace_mapto(): vs=%p, va=%p, virt=%p, flags=0x%x", &vs, va, virt, flags);
	va_out = va;

	/* Provide a mapping for the pages */
	md_map_pages(&vs, va->va_virt, 0, BytesToPages(len), 0); //(flags & VM_FLAG_FAULT) ? 0 : flags);
	return ananas_success();
}

errorcode_t
vmspace_mapto_dentry(VMSpace& vs, addr_t virt, size_t vlength, struct DENTRY* dentry, off_t doffset, size_t dlength, int flags, VMArea*& va_out)
{
	// Ensure the range we are mapping does not exceed the inode; if this is the case, we silently truncate
	if (doffset + vlength > dentry->d_inode->i_sb.st_size) {
		vlength = dentry->d_inode->i_sb.st_size - doffset;
	}

	KASSERT((doffset & (PAGE_SIZE - 1)) == 0, "offset %d not page-aligned", doffset);

	errorcode_t err = vmspace_mapto(vs, virt, vlength, flags | VM_FLAG_FAULT, va_out);
	ANANAS_ERROR_RETURN(err);

	dentry_ref(dentry);
	va_out->va_dentry = dentry;
	va_out->va_doffset = doffset;
	va_out->va_dlength = dlength;
	return ananas_success();
}

errorcode_t
vmspace_map(VMSpace& vs, size_t len /* bytes */, uint32_t flags, VMArea*& va_out)
{
	return vmspace_mapto(vs, vmspace_determine_va(vs, len), len, flags, va_out);
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
vmspace_clone_area_must_free(const VMArea& va, int flags)
{
	/* Scenario (2) does not free MD-specific parts */
	if ((flags & VMSPACE_CLONE_EXEC) && (va.va_flags & VM_FLAG_MD))
		return 0;
	return (va.va_flags & VM_FLAG_NO_CLONE) == 0;
}

static inline int
vmspace_clone_area_must_copy(const VMArea& va, int flags)
{
	/* Scenario (2) does copy MD-specific parts */
	if ((flags & VMSPACE_CLONE_EXEC) && (va.va_flags & VM_FLAG_MD))
		return 1;
	return (va.va_flags & VM_FLAG_NO_CLONE) == 0;
}

errorcode_t
vmspace_clone(VMSpace& vs_source, VMSpace& vs_dest, int flags)
{
	TRACE(VM, INFO, "vmspace_clone(): source=%p dest=%p flags=%x", &vs_source, &vs_dest, flags);

	/*
	 * First, clean up the destination area's mappings - this ensures we'll
	 * overwrite them with our own. Note that we'll leave private mappings alone.
	 *
	 * Note that this changes vs_areas while we are iterating through it, so we
	 * need to increment the iterator beforehand!
	 */
	for(auto it = vs_dest.vs_areas.begin(); it != vs_dest.vs_areas.end(); /* nothing */) {
		VMArea& va = *it; ++it;
		if (!vmspace_clone_area_must_free(va, flags))
			continue;
		vmspace_area_free(vs_dest, va);
	}

	/* Now copy everything over that isn't private */
	for(auto& va_src: vs_source.vs_areas) {
		if (!vmspace_clone_area_must_copy(va_src, flags))
			continue;

		VMArea* va_dst;
		errorcode_t err = vmspace_mapto(vs_dest, va_src.va_virt, va_src.va_len, va_src.va_flags, va_dst);
		ANANAS_ERROR_RETURN(err);
		if (va_src.va_dentry != nullptr) {
			// Backed by an inode; copy the necessary fields over
			va_dst->va_doffset = va_src.va_doffset;
			va_dst->va_dlength = va_src.va_dlength;
			va_dst->va_dentry = va_src.va_dentry;
			dentry_ref(va_dst->va_dentry);
		}

		// Copy the area page-wise
		for(auto& vp: va_src.va_pages) {
			vmpage_lock(vp);
			KASSERT(vmpage_get_page(vp)->p_order == 0, "unexpected %d order page here", vmpage_get_page(vp)->p_order);

			// Create a clone of the data; it is up to the vmpage how to do this (it may go for COW)
			VMPage& new_vp = vmpage_clone(vs_source, va_src, *va_dst, vp);

			// Map the page into the cloned vmspace
			vmpage_map(vs_dest, *va_dst, new_vp);
			vmpage_unlock(new_vp);
			vmpage_unlock(vp);
		}
	}

	/*
	 * See where the next mapping can be placed; we should use something more
	 * clever than vs_next_mapping someday but this will have to suffice for
	 * now.
	 */
	vs_dest.vs_next_mapping = 0;
	for(auto& va: vs_dest.vs_areas) {
		addr_t next = RoundUp(va.va_virt + va.va_len);
		if (vs_dest.vs_next_mapping < next)
			vs_dest.vs_next_mapping = next;
	}

	return ananas_success();
}

void
vmspace_area_free(VMSpace& vs, VMArea& va)
{
	vs.vs_areas.remove(va);

	/* Free any backing dentry, if we have one */
	if (va.va_dentry != nullptr)
		dentry_deref(va.va_dentry);

	/*
	 * If the pages were allocated, we need to free them one by one
	 *
	 * Note that this changes va_pages while we are iterating through it, so we
	 * need to increment the iterator beforehand!
	 */
	for (auto it = va.va_pages.begin(); it != va.va_pages.end(); /* nothing */) {
		VMPage& vp = *it; ++it;
		vmpage_lock(vp);
		vmpage_deref(vp);
	}
	delete &va;
}

void
vmspace_dump(VMSpace& vs)
{
	for(auto& va: vs.vs_areas) {
		kprintf("  area %p: %p..%p flags %c%c%c%c%c%c%c%c\n",
		 &va, va.va_virt, va.va_virt + va.va_len - 1,
		 (va.va_flags & VM_FLAG_READ) ? 'r' : '.',
		 (va.va_flags & VM_FLAG_WRITE) ? 'w' : '.',
		 (va.va_flags & VM_FLAG_EXECUTE) ? 'x' : '.',
		 (va.va_flags & VM_FLAG_KERNEL) ? 'k' : '.',
		 (va.va_flags & VM_FLAG_USER) ? 'u' : '.',
		 (va.va_flags & VM_FLAG_PRIVATE) ? 'p' : '.',
		 (va.va_flags & VM_FLAG_NO_CLONE) ? 'n' : '.',
		 (va.va_flags & VM_FLAG_MD) ? 'm' : '.');
		kprintf("    pages:\n");
		for(auto& vp: va.va_pages) {
			vmpage_dump(vp, "      ");
		}
	}
}

/* vim:set ts=2 sw=2: */
