#include <ananas/types.h>
#include <machine/param.h> /* for PAGE_SIZE */
#include <machine/vm.h> /* for md_{,un}map_pages() */
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/trace.h>
#include <ananas/vm.h>
#include <ananas/vfs/core.h>
#include <ananas/vmspace.h>

TRACE_SETUP;

namespace {

errorcode_t
read_data(struct DENTRY* dentry, void* buf, off_t offset, size_t len)
{
	struct VFS_FILE f;
	memset(&f, 0, sizeof(f));
	f.f_dentry = dentry;

	errorcode_t err = vfs_seek(&f, offset);
	ANANAS_ERROR_RETURN(err);

	size_t amount = len;
	err = vfs_read(&f, buf, &amount);
	ANANAS_ERROR_RETURN(err);

	if (amount != len)
		return ANANAS_ERROR(SHORT_READ);
	return ananas_success();
}

errorcode_t
vmspace_handle_fault_dentry(vmspace_t* vs, vmarea_t* va, addr_t virt)
{
	/*
	 * Calculate what to read from where; we must fault per page, so we need to
	 * make sure the entire page is sane upon return.
	 *
	 * The idea is to read 'read_addr' bytes to 'read_off'. Everything else we
	 * will zero out.
	 */
	addr_t v_page = virt & ~(PAGE_SIZE - 1);
	addr_t read_addr = v_page;
	off_t read_off = v_page - va->va_virt; // offset in area, still needs va_doffset added
	size_t read_len = PAGE_SIZE;

	// Now clip the read_off/read_len values so we only hit what we are allowed
	if (read_off < va->va_dvskip) {
		// We aren't allowed to read here, so shift a bit (this assumes that va_dvskip is always < PAGE_SIZE)
		read_addr += va->va_dvskip;
		read_len -= va->va_dvskip;
	}
	if (read_off + read_len > va->va_dlength) {
		if (va->va_dlength > read_off)
			read_len = va->va_dlength - read_off;
		else
			read_len = 0;
	}

	TRACE(VM, INFO, "vmspace_handle_fault_dentry(): va=%p, v=%p, reading %d bytes @ %p to %p",
	 va, virt, (unsigned int)read_len, (unsigned int)(read_off + va->va_doffset), read_addr);

	if (read_addr != v_page || read_len != PAGE_SIZE)
		memset((void*)v_page, 0, PAGE_SIZE);
	if (read_len > 0)
		return read_data(va->va_dentry, (void*)read_addr, read_off + va->va_doffset, read_len);
	return ananas_success();
}

} // unnamed namespace

errorcode_t
vmspace_handle_fault(vmspace_t* vs, addr_t virt, int flags)
{
	TRACE(VM, INFO, "vmspace_handle_fault(): vs=%p, virt=%p, flags=0x%x", vs, virt, flags);

	/* Walk through the areas one by one */
	LIST_FOREACH(&vs->vs_areas, va, vmarea_t) {
		if (!(virt >= va->va_virt && (virt < (va->va_virt + va->va_len))))
			continue;

		/* We should only get faults for lazy areas (filled by a function) or when we have to dynamically allocate things */
		KASSERT((va->va_flags & (VM_FLAG_ALLOC | VM_FLAG_LAZY)) != 0, "unexpected pagefault in area %p, virt=%p, len=%d, flags 0x%x", va, va->va_virt, va->va_len, va->va_flags);

		/* Allocate a new page; this will be used to handle the fault */
		struct PAGE* p = page_alloc_single();
		if (p == NULL)
			return ANANAS_ERROR(OUT_OF_MEMORY);
		LIST_APPEND(&va->va_pages, p);
		p->p_addr = virt & ~(PAGE_SIZE - 1);

		/* Map the page */
		md_map_pages(vs, p->p_addr, page_get_paddr(p), 1, va->va_flags);
		errorcode_t err = ananas_success();
		if (va->va_dentry != nullptr) {
			err = vmspace_handle_fault_dentry(vs, va, virt);
			if (ananas_is_failure(err)) {
				/* Mapping failed; throw the thread mapping away and nuke the page */
				md_unmap_pages(vs, p->p_addr, 1);
				LIST_REMOVE(&va->va_pages, p);
				page_free(p);
			}
		}
		return err;
	}

	return ANANAS_ERROR(BAD_ADDRESS);
}

/* vim:set ts=2 sw=2: */
