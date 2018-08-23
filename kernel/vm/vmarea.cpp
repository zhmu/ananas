#include "kernel/vmpage.h"
#include "kernel/vmarea.h"
#include "kernel/page.h"
#include "kernel/lib.h"

#include "kernel/vmspace.h"

#define DPRINTF(...)

VMPage*
VMArea::LookupVAddrAndLock(addr_t vaddr)
{
	for(auto& vmpage: va_pages) {
		if (vmpage.vp_vaddr != vaddr)
			continue;

		vmpage.Lock();
		return &vmpage;
	}

	// Nothing was found
	return nullptr;
}

VMPage&
VMArea::AllocatePrivatePage(addr_t va, int flags)
{
	KASSERT((flags & VM_PAGE_FLAG_COW) == 0, "allocating cow page here?");
	KASSERT((flags & VM_PAGE_FLAG_LINK) == 0, "allocating linked page here?");
	KASSERT((flags & VM_PAGE_FLAG_PENDING) == 0, "allocating pending page here?");

	flags |= VM_PAGE_FLAG_PRIVATE;
  auto& new_page = vmpage::Allocate(*this, nullptr, 0, flags);

  // Hook a page to here as well, as the caller needs it anyway
	new_page.vp_vaddr = va;
  new_page.vp_page = page_alloc_single();
	KASSERT(new_page.vp_page != nullptr, "out of pages");
  return new_page;
}

VMPage&
VMArea::PromotePage(VMPage& vp)
{
	KASSERT(vp.vp_vmarea == this, "wrong vmarea");

  // This promotes a COW page to a new writable page
  vp.AssertLocked();
  KASSERT((vp.vp_flags & VM_PAGE_FLAG_COW) != 0, "attempt to promote non-COW page");
	KASSERT((vp.vp_flags & VM_PAGE_FLAG_READONLY) == 0, "cowing r/o page");
	if ((vp.vp_flags & VM_PAGE_FLAG_PROMOTED) != 0) {
		//dump_ptab(va_vs.vs_md_pagedir);
		KASSERT((vp.vp_flags & VM_PAGE_FLAG_PROMOTED) == 0, "promoting promoted page %p", &vp);
	}
  KASSERT(vp.vp_refcount > 0, "invalid refcount of vp");

  // See if this page is linked; if not, it makes things a lot easier
	if ((vp.vp_flags & VM_PAGE_FLAG_LINK) == 0) {
		/*
		 * No linked page in between - we now either:
		 * (a) [refcount==1] Need to just mark the current page as non-COW, as it is the last one
		 * (b) [refcount>1]  Allocate a new page, copying the current data to it
		 */
  	if (vp.vp_refcount == 1) {
      // (a) We *are* the source page! We can just use it
      DPRINTF("vmpage_promote(): vp %p, we are the last page - using it! (page %p @ %p)\n", &vp, vp.vp_page, vp.vp_vaddr);
			vp.vp_flags = (vp.vp_flags & ~VM_PAGE_FLAG_COW) | VM_PAGE_FLAG_PRIVATE;
			return vp;
		}

		// (b) We have the original page - must allocate a new one, as we can't
		// touch this one (it is used by other refs we cannot touch)
    auto& new_vp = AllocatePrivatePage(vp.vp_vaddr, vp.vp_flags & ~VM_PAGE_FLAG_COW);
    DPRINTF("vmpage_promote(): made new vp %p page %p for %p @ %p\n", &new_vp, new_vp.vp_page, &vp, new_vp.vp_vaddr);
		new_vp.vp_flags |= VM_PAGE_FLAG_PROMOTED;

		vp.Copy(new_vp);

		/*
		 * Sever the connection to the vmarea; we have made a copy of the page
		 * itself, so it no longer belongs there. This prevents it from being
		 * shared during a clone and such.
		 */
  	va_pages.remove(vp);
		vp.vp_vmarea = nullptr;
		vp.Deref();
		new_vp.AssertNotLinked();

		return new_vp;
	}

	// vp and vp_source are not the same; this means vp is linked to vp_source
  auto& vp_source = vp.Resolve();
  vp_source.Lock();
  KASSERT(vp_source.vp_refcount > 0, "invalid refcount of source");
	vp_source.AssertNotLinked();
  KASSERT((vp_source.vp_flags & VM_PAGE_FLAG_READONLY) == 0, "cowing r/o page");

  /*
   * Multiple scenario's here:
   *
   * (1) We hold the last reference to the source page
   *     This means we can re-use the page it contains and free the source.
   * (2) The source page still has >1 reference
   *     We need to copy the contents to a fresh new page
   */
  if (vp_source.vp_refcount == 1) {
    /*
		 * (1) We hold the only reference. However, this is tricky because there is
		 *     a linked page in between (since vp != vp_source), which means we can
		 *     grab the page from vp_source and put it in vp (vp_source will be
		 *     destroyed)
     */
		DPRINTF("vmpage_promote(): vp %p, last user %p @ %p -> stealing page %p\n", &vp_source, &vp, vp.vp_vaddr, vp_source.vp_page);
		// Steal the page from the source...
		vp.vp_page = vp_source.vp_page;
		vp_source.vp_page = nullptr;

		// ... which we can now destroy
		vp_source.Deref();

		// We're no longer linked, nor COW
		vp.vp_flags &= ~(VM_PAGE_FLAG_LINK | VM_PAGE_FLAG_COW);
		vp.vp_flags |= VM_PAGE_FLAG_PROMOTED;
		return vp;
  } else /* vp_source.vp_refcount > 1 */ {
    /* (2) - multiple references, need to make a copy */
    KASSERT((vp.vp_flags & VM_PAGE_FLAG_LINK) != 0, "destination vp not linked?");

		// We need to allocate a new backing page for the destination and hook it up
		vp.vp_page = page_alloc_single();
		KASSERT(vp.vp_page != nullptr, "out of pages");
		vp.vp_flags &= ~VM_PAGE_FLAG_LINK;

		// And we can continue copying things into it
		DPRINTF("vmpage_promote(): vp %p, must copy page %p -> page %p @ %p!\n", &vp, vp_source.vp_page, vp.vp_page, vp.vp_vaddr);

    // Copy the data over and throw away the source; this never deletes it
    vp_source.Copy(vp);
    vp_source.Deref();

		vp.vp_flags = (vp.vp_flags & ~VM_PAGE_FLAG_COW) | VM_PAGE_FLAG_PRIVATE;
		vp.vp_flags |= VM_PAGE_FLAG_PROMOTED;
		return vp;
  }

  // NOTREACHED
}

/* vim:set ts=2 sw=2: */
