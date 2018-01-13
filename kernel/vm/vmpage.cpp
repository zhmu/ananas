#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/vmpage.h"
#include "kernel/vmspace.h"
#include "kernel/vfs/types.h"
#include "kernel/vm.h"
#include "kernel-md/vm.h" // for md_{,un}map_pages()

#define DEBUG 0

#if DEBUG

#define DPRINTF kprintf

#include "kernel/pcpu.h"
#include "kernel/process.h"

static inline int get_pid()
{
  process_t* p = PCPU_GET(curthread)->t_process;
  return p != nullptr ? p->p_pid : 0;
}

#else
#define DPRINTF(...)
#endif

namespace {

void
vmpage_free(VMPage& vmpage)
{
  vmpage_assert_locked(vmpage);

  KASSERT(vmpage.vp_refcount == 0, "freeing page with refcount %d", vmpage.vp_refcount);

  DPRINTF("[%d] vmpage_free(): vp %p @ %p (page %p phys %p)\n", get_pid(), &vmpage, vmpage.vp_vaddr,
    (vmpage.vp_flags & VM_PAGE_FLAG_LINK) == 0 ? vmpage.vp_page : 0,
    (vmpage.vp_flags & VM_PAGE_FLAG_LINK) == 0 && vmpage.vp_page != nullptr ? page_get_paddr(vmpage.vp_page) : 0);

  // Note that we do not hold any references to the inode (the inode owns us)
  if (vmpage.vp_flags & VM_PAGE_FLAG_LINK) {
    if (vmpage.vp_link != nullptr) {
      vmpage_lock(*vmpage.vp_link);
      vmpage_deref(*vmpage.vp_link);
    }
  } else {
    if (vmpage.vp_page != nullptr)
      page_free(vmpage.vp_page);
  }

  // If we are hooked to a vmarea, unlink us
  if (vmpage.vp_vmarea != nullptr)
    vmpage.vp_vmarea->va_pages.remove(vmpage);
  delete &vmpage;
}

VMPage&
vmpage_resolve(VMPage& v)
{
	VMPage* vp = &v;
  if (vp->vp_flags & VM_PAGE_FLAG_LINK) {
    vp = vp->vp_link;
    KASSERT((vp->vp_flags & VM_PAGE_FLAG_LINK) == 0, "link to a linked page");
  }
  return *vp;
}

VMPage&
vmpage_resolve_locked(VMPage& vp)
{
  vmpage_assert_locked(vp);

  auto& vp_resolved = vmpage_resolve(vp);
  if (&vp_resolved == &vp)
    return vp;
  vmpage_lock(vp_resolved);

  vmpage_unlock(vp);
  return vp_resolved;
}

VMPage&
vmpage_alloc(VMArea* va, struct VFS_INODE* inode, off_t offset, int flags)
{
  auto vp = new VMPage;
	memset(vp, 0, sizeof(*vp));
  mutex_init(&vp->vp_mtx, "vmpage");
  vp->vp_vmarea = va;
  vp->vp_inode = inode;
  vp->vp_offset = offset;
  vp->vp_flags = flags;
  vp->vp_refcount = 1; // caller

  vmpage_lock(*vp);
  if (va != nullptr)
		va->va_pages.push_back(*vp);
  return *vp;
}

VMPage&
vmpage_clone_cow(VMSpace& vs, VMArea& va, VMPage& v)
{
  //KASSERT((vp->vp_flags & VM_PAGE_FLAG_COW) == 0, "trying to clone cow page %p that is already cow", vp);

  // Get the virtual address before dereferencing links as the source is never mapped
  addr_t vaddr = v.vp_vaddr;
  auto& vp = vmpage_resolve_locked(v);

  // Mark the source page as COW and update the mapping - this makes it read-only
  vp.vp_flags |= VM_PAGE_FLAG_COW;
  vmpage_map(vs, va, vp);

  // Return a link to the page, but do mark it as COW as well
  auto& vp_new = vmpage_link(va, vp);
  vp_new.vp_flags |= VM_PAGE_FLAG_COW;
  vp_new.vp_vaddr = vaddr;
  return vp_new;
}

} // unnamed namespace

VMPage&
vmpage_link(VMArea& va, VMPage& vp)
{
  vmpage_assert_locked(vp);
  VMPage& vp_source = vmpage_resolve_locked(vp);

  // Increase source refcount as we will be linked towards it
  vmpage_ref(vp_source);

  int flags = VM_PAGE_FLAG_LINK;
  if (vp_source.vp_flags & VM_PAGE_FLAG_READONLY)
    flags |= VM_PAGE_FLAG_READONLY;

  auto& vp_new = vmpage_alloc(&va, vp_source.vp_inode, vp_source.vp_offset, flags);
  vp_new.vp_link = &vp_source;
  vp_new.vp_vaddr = vp_source.vp_vaddr;

  if (&vp_source != &vp) {
    vmpage_unlock(vp_source);
    vmpage_lock(vp);
  }
  return vp_new;
}

void
vmpage_copy_extended(VMPage& vp_src, VMPage& vp_dst, size_t len)
{
  vmpage_assert_locked(vp_src);
  vmpage_assert_locked(vp_dst);
  KASSERT(&vp_src != &vp_dst, "copying same vmpage %p", &vp_src);
  KASSERT(len <= PAGE_SIZE, "copying more than a page");

  struct PAGE* p_src = vmpage_get_page(vp_src);
  struct PAGE* p_dst = vmpage_get_page(vp_dst);
  KASSERT(p_src != p_dst, "copying same page %p", p_src);

  auto src = static_cast<char*>(kmem_map(page_get_paddr(p_src), PAGE_SIZE, VM_FLAG_READ));
  auto dst = static_cast<char*>(kmem_map(page_get_paddr(p_dst), PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE));

	memcpy(dst, src, len);
  if (len < PAGE_SIZE)
    memset(dst + len, 0, PAGE_SIZE - len); // zero-fill after the data to be copied

  kmem_unmap(dst, PAGE_SIZE);
  kmem_unmap(src, PAGE_SIZE);
}

VMPage&
vmpage_promote(VMSpace& vs, VMArea& va, VMPage& vp)
{
  // This promotes a COW page to a new writable page
  vmpage_assert_locked(vp);
  KASSERT((vp.vp_flags & VM_PAGE_FLAG_COW) != 0, "attempt to promote non-COW page");

  // Get a reference to the source page - this is what we need to copy
  auto& vp_source = vmpage_resolve(vp);
  if (&vp_source != &vp)
    vmpage_lock(vp_source); // freed by vmpage_deref()
  KASSERT(vp.vp_refcount > 0, "invalid refcount of vp");
  KASSERT(vp_source.vp_refcount > 0, "invalid refcount of source");

  /*
   * Multiple scenario's here:
   *
   * (1) We hold the last reference to to the source page
   *     This means we can re-use the page it contains and free the source.
   * (2) The source page still have >1 reference
   *     We need to copy the contents to a fresh new page
   */
	VMPage* new_vp = &vp;
  if (vp_source.vp_refcount == 1) {
    /*
     * (1) We hold the only reference. However, this is tricky because there may be
     *     a linked page in between.
     */
    if (&vp != &vp_source) {
      DPRINTF("%d: vmpage_promote(): vp %p, last user %p @ %p -> stealing page %p\n", get_pid(), &vp_source, &vp, vp.vp_vaddr, vp_source.vp_page);
      // Steal the page from the source...
      vp.vp_page = vp_source.vp_page;
      vp_source.vp_page = nullptr;

      // ... which we can now destroy
      vmpage_deref(vp_source);

      // We're no longer linked, either
      vp.vp_flags &= ~VM_PAGE_FLAG_LINK;
    } else /* &vp_source == &vp - we are not linked */ {
      // We *are* the source page! We can just use it
      DPRINTF("%d: vmpage_promote(): vp %p, we are the last page - using it! (page %p @ %p)\n", get_pid(), &vp, vp.vp_page, vp.vp_vaddr);
    }
  } else /* vp_source.vp_refcount > 1 */ {
    /* (2) - multiple references, need to make a copy */
    if (&vp_source == &vp) {
      // We have the original page - must allocate a new one, as we can't touch this one
      new_vp = &vmpage_create_private(&va, vp_source.vp_flags | VM_PAGE_FLAG_PRIVATE);
      new_vp->vp_vaddr = vp_source.vp_vaddr;

      // Remove the original source from the vmarea
      vp_source.vp_vmarea->va_pages.remove(vp_source);
      vp_source.vp_vmarea = nullptr;

      DPRINTF("%d: vmpage_promote(): made new vp %p page %p for %p @ %p\n", get_pid(), new_vp, new_vp->vp_page, &vp_source, new_vp->vp_vaddr);
    } else /* &vp_source != &vp */ {
      KASSERT((vp.vp_flags & VM_PAGE_FLAG_LINK) != 0, "destination vp not linked?");

      // We need t allocate a new page for the destination and hook it up
      vp.vp_page = page_alloc_single();
      KASSERT(vp.vp_page != nullptr, "out of pages");
      vp.vp_flags &= ~VM_PAGE_FLAG_LINK;

      // And we can continue copying things into it
      DPRINTF("%d: vmpage_promote(): vp %p, must copy page %p -> page %p @ %p!\n", get_pid(), &vp, vp_source.vp_page, vp.vp_page, vp.vp_vaddr);
    }

    // Copy the data over and throw away the source; this never deletes it
    vmpage_copy(vp_source, *new_vp);
    vmpage_deref(vp_source);
  }

  // Our page is no longer COW, but it is private. Note that we never unlock vp here
  new_vp->vp_flags = (new_vp->vp_flags & ~VM_PAGE_FLAG_COW) | VM_PAGE_FLAG_PRIVATE;
  return *new_vp;
}

void
vmpage_ref(VMPage& vmpage)
{
  vmpage_assert_locked(vmpage);
  ++vmpage.vp_refcount;
}

void
vmpage_deref(VMPage& vmpage)
{
  vmpage_assert_locked(vmpage);

  KASSERT(vmpage.vp_refcount > 0, "invalid refcount %d", vmpage.vp_refcount);
  if (--vmpage.vp_refcount > 0) {
    vmpage_unlock(vmpage);
    return;
  }

  vmpage_free(vmpage);
}

VMPage*
vmpage_lookup_locked(VMArea& va, struct VFS_INODE* inode, off_t offs)
{
  /*
   * First step is to see if we can locate this page for the given vmspace - the private mappings
   * are stored there and override global ones.
   */
	for(auto& vmpage: va.va_pages) {
    if (vmpage.vp_inode != inode || vmpage.vp_offset != offs)
      continue;

    vmpage_lock(vmpage);
    return &vmpage;
  }

  // Try all inode-private pages
	INODE_LOCK(inode);
	for(auto& vmpage: inode->i_pages) {
    // We don't check vp_inode here as this is the per-inode list already
    if (vmpage.vp_offset != offs)
      continue;

    vmpage_lock(vmpage); // XXX is this order wise?
    INODE_UNLOCK(inode);
    return &vmpage;
  }
	INODE_UNLOCK(inode);

  // Nothing was found
  return nullptr;
}

VMPage*
vmpage_lookup_vaddr_locked(VMArea& va, addr_t vaddr)
{
	for(auto& vmpage: va.va_pages) {
    if (vmpage.vp_vaddr != vaddr)
      continue;

    vmpage_lock(vmpage);
    return &vmpage;
  }

  // Nothing was found
  return nullptr;
}

VMPage&
vmpage_clone(VMSpace& vs, VMArea& va_source, VMArea& va_dest, VMPage& vp_orig)
{
  vmpage_assert_locked(vp_orig);

  /*
   * Cloning a page may results in different scenarios:
   *
   * 1) We can create a link to the source page, using the same access
   * 2) We can create a link to the source page, yet employ COW
   *    This consists of marking the original page as read-only.
   * 3) We need to duplicate the source page
   */
  auto& vp_source = vmpage_resolve_locked(vp_orig);
  KASSERT((vp_source.vp_flags & VM_PAGE_FLAG_PENDING) == 0, "trying to clone a pending page");

  // (3) If this is a MD-specific page, just duplicate it - we don't want to share things like
  // pagetables and the like
  VMPage* vp_dst;
  if (va_source.va_flags & VM_FLAG_MD) {
    vp_dst = &vmpage_create_private(&va_dest, vp_source.vp_flags | VM_PAGE_FLAG_PRIVATE);
    vp_dst->vp_vaddr = vp_source.vp_vaddr;
    vmpage_copy(vp_source, *vp_dst);
  } else if (vp_source.vp_flags & VM_PAGE_FLAG_READONLY) {
    // (1) If the source is read-only, we can always share it
    vp_dst = &vmpage_link(va_dest, vp_source);
  } else {
    // (2) Clone the page using COW
    vp_dst = &vmpage_clone_cow(vs, va_dest, vp_source);
  }

  if (&vp_orig != &vp_source) {
    vmpage_unlock(vp_source);
    vmpage_lock(vp_orig);
  }
  return *vp_dst;
}

struct PAGE*
vmpage_get_page(VMPage& v)
{
	VMPage* vp = &v;
  if (vp->vp_flags & VM_PAGE_FLAG_LINK) {
    vp = vp->vp_link;
    KASSERT((vp->vp_flags & VM_PAGE_FLAG_LINK) == 0, "link to a linked page");
  }

  return vp->vp_page;
}

VMPage&
vmpage_create_shared(struct VFS_INODE* inode, off_t offs, int flags)
{
  VMPage& new_page = vmpage_alloc(nullptr, inode, offs, flags);

  // Hook the vm page to the inode
  INODE_LOCK(inode);
	for(auto& vmpage: inode->i_pages) {
    if (vmpage.vp_offset != offs || &vmpage == &new_page)
      continue;

    // Page is already present - return the one already in use
    vmpage_lock(vmpage); // XXX is this order wise?
    INODE_UNLOCK(inode);

    // And throw the new page away, we won't need it
    vmpage_unlock(new_page);
    delete &new_page;
    return vmpage;
  }

  // Not yet present; add the new page and return it
	inode->i_pages.push_back(new_page);
  INODE_UNLOCK(inode);
  return new_page;
}

VMPage&
vmpage_create_private(VMArea* va, int flags)
{
  auto& new_page = vmpage_alloc(va, nullptr, 0, flags);

  // Hook a page to here as well, as the caller needs it anyway
  new_page.vp_page = page_alloc_single();
	KASSERT(new_page.vp_page != nullptr, "out of pages");
  return new_page;
}

void
vmpage_map(VMSpace& vs, VMArea& va, VMPage& vp)
{
	vmpage_assert_locked(vp);

	int flags = va.va_flags;
	// Map COW pages as unwritable so we'll fault on a write
	if (vp.vp_flags & VM_PAGE_FLAG_COW)
		flags &= ~VM_FLAG_WRITE;
	struct PAGE* p = vmpage_get_page(vp);
	md_map_pages(&vs, vp.vp_vaddr, page_get_paddr(p), 1, flags);
}

void
vmpage_zero(VMSpace& vs, VMPage& vp)
{
	// Clear the page XXX This is unfortunate, we should have a supply of pre-zeroed pages
	struct PAGE* p = vmpage_get_page(vp);
	md_map_pages(&vs, vp.vp_vaddr, page_get_paddr(p), 1, VM_FLAG_READ | VM_FLAG_WRITE);
	memset((void*)vp.vp_vaddr, 0, PAGE_SIZE);
}

void vmpage_dump(const VMPage& vp, const char* prefix)
{
  kprintf("%s%p: refcount %d vaddr %p flags %s/%s/%s/%c ",
    prefix, &vp, vp.vp_refcount,
    vp.vp_vaddr,
    (vp.vp_flags & VM_PAGE_FLAG_PRIVATE) ? "prv" : "pub",
    (vp.vp_flags & VM_PAGE_FLAG_READONLY) ? "ro" : "rw",
    (vp.vp_flags & VM_PAGE_FLAG_COW) ? "cow" : "---",
    (vp.vp_flags & VM_PAGE_FLAG_PENDING) ? 'p' : '.');
  if (vp.vp_flags & VM_PAGE_FLAG_LINK) {
    kprintf(" -> ");
    vmpage_dump(*vp.vp_link, "");
    return;
  }
  if (vp.vp_page != nullptr)
    kprintf("%spage %p phys %p order %d", prefix, vp.vp_page, page_get_paddr(vp.vp_page), vp.vp_page->p_order);
  kprintf("\n");
}

/* vim:set ts=2 sw=2: */
