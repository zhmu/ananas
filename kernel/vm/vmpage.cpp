#include <ananas/types.h>
#include <ananas/vmpage.h>
#include <ananas/init.h>
#include <ananas/lock.h>
#include <ananas/mm.h>
#include <ananas/lib.h>
#include <ananas/vmspace.h>
#include <ananas/error.h>
#include <ananas/vfs/types.h>
#include <ananas/kmem.h>
#include <ananas/vm.h>
#include <machine/param.h> // for PAGE_SIZE
#include <machine/vm.h> /* for md_{,un}map_pages() */

namespace {

void
vmpage_free(struct VM_PAGE* vmpage)
{
  // Note that we do not hold any references to the inode (the inode owns us)
  if (vmpage->vp_flags & VM_PAGE_FLAG_LINK) {
    if (vmpage->vp_link != nullptr)
      vmpage_deref(vmpage->vp_link);
  } else {
    if (vmpage->vp_page != nullptr)
      page_free(vmpage->vp_page);
  }
  kfree(vmpage);
}

#if 0
void
vmpage_promote(struct VM_PAGE* vp)
{
  // This promotes a COW page to a new writable page
  KASSERT((vp->vp_flags & VM_PAGE_FLAG_COW) != 0, "attempt to promote non-COW page");

  panic("todo");
}
#endif

struct VM_PAGE*
vmpage_alloc(struct VFS_INODE* inode, off_t offset, int flags)
{
  auto vp = static_cast<struct VM_PAGE*>(kmalloc(sizeof(struct VM_PAGE)));
  memset(vp, 0, sizeof(struct VM_PAGE));
  mutex_init(&vp->vp_mtx, "vmpage");
  vp->vp_inode = inode;
  vp->vp_offset = offset;
  vp->vp_flags = flags;
  vp->vp_refcount = 1; // caller

  return vp;
}

} // unnamed namespace

struct VM_PAGE*
vmpage_link(struct VM_PAGE* vp)
{
  // If the source is a link itself, dereference it
  if (vp->vp_flags & VM_PAGE_FLAG_LINK) {
    vp = vp->vp_link;
    KASSERT((vp->vp_flags & VM_PAGE_FLAG_LINK) == 0, "link to a linked page");
  }

  // Increase source refcount as we will be linked towards it
  vmpage_ref(vp);

  int flags = VM_PAGE_FLAG_LINK;
  if (vp->vp_flags & VM_PAGE_FLAG_READONLY)
    flags |= VM_PAGE_FLAG_READONLY;

  struct VM_PAGE* vp_new = vmpage_alloc(vp->vp_inode, vp->vp_offset, flags);
  vp_new->vp_link = vp;
  return vp_new;
}

void
vmpage_copy(struct VM_PAGE* vp_src, struct VM_PAGE* vp_dst)
{
  struct PAGE* p_src = vmpage_get_page(vp_src);
  struct PAGE* p_dst = vmpage_get_page(vp_dst);

  void* src = kmem_map(page_get_paddr(p_src), PAGE_SIZE, VM_FLAG_READ);
  void* dst = kmem_map(page_get_paddr(p_dst), PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE);
	memcpy(dst, src, PAGE_SIZE);
  kmem_unmap(dst, PAGE_SIZE);
  kmem_unmap(src, PAGE_SIZE);
}

void
vmpage_ref(struct VM_PAGE* vmpage)
{
  ++vmpage->vp_refcount;
}

void
vmpage_deref(struct VM_PAGE* vmpage)
{
  KASSERT(vmpage->vp_refcount > 0, "invalid refcount %d", vmpage->vp_refcount);
  if (--vmpage->vp_refcount > 0)
    return;

  vmpage_free(vmpage);
}

struct VM_PAGE*
vmpage_lookup_locked(vmarea_t* va, struct VFS_INODE* inode, off_t offs)
{
  /*
   * First step is to see if we can locate this page for the given vmspace - the private mappings
   * are stored there and override global ones.
   */
  LIST_FOREACH(&va->va_pages, vmpage, struct VM_PAGE) {
    if (vmpage->vp_inode != inode || vmpage->vp_offset != offs)
      continue;

    vmpage_lock(vmpage);
    return vmpage;
  }

  // Try all inode-private pages
	INODE_LOCK(inode);
  LIST_FOREACH(&inode->i_pages, vmpage, struct VM_PAGE) {
    // We don't check vp_inode here as this is the per-inode list already
    if (vmpage->vp_offset != offs)
      continue;

    vmpage_lock(vmpage); // XXX is this order wise?
    INODE_UNLOCK(inode);
    return vmpage;
  }
	INODE_UNLOCK(inode);

  // Nothing was found
  return nullptr;
}

struct VM_PAGE*
vmpage_clone(struct VM_PAGE* vp_source)
{
  // Get the virtual address before dereferencing links as the source is never mapped
  addr_t vaddr = vp_source->vp_vaddr;

  /*
   * Cloning a page may results in different scenarios:
   *
   * 1) We can create a link to the source page, using the same access
   * 2) We can create a link to the source page, yet employ COW
   *    This consists of marking the original page as read-only.
   * 3) We need to duplicate the source page
   */
  if (vp_source->vp_flags & VM_PAGE_FLAG_LINK) {
    // Linked page - get the reference to the original
    vp_source = vp_source->vp_link;
    KASSERT((vp_source->vp_flags & VM_PAGE_FLAG_LINK) == 0, "link to a linked page");
  }
  KASSERT((vp_source->vp_flags & VM_PAGE_FLAG_PENDING) == 0, "trying to clone a pending page");

  // (1) If the source is read-only, we can always share it
  if (vp_source->vp_flags & VM_PAGE_FLAG_READONLY) 
    return vmpage_link(vp_source);
  
  // TODO Implement (2) !

  // (3) Make a copy of the page; it will always become a private page now since it won't
  //     be shared
  struct VM_PAGE* vp_dst = vmpage_create_private(vp_source->vp_flags | VM_PAGE_FLAG_PRIVATE /* XXX? is this ok? */);
  vp_dst->vp_vaddr = vaddr;
	vmpage_copy(vp_source, vp_dst);
  return vp_dst;
}

struct PAGE*
vmpage_get_page(struct VM_PAGE* vp)
{
  if (vp->vp_flags & VM_PAGE_FLAG_LINK) {
    vp = vp->vp_link;
    KASSERT((vp->vp_flags & VM_PAGE_FLAG_LINK) == 0, "link to a linked page");
  }

  return vp->vp_page;
}

struct VM_PAGE*
vmpage_create_shared(vmarea_t* va, struct VFS_INODE* inode, off_t offs, int flags)
{
  struct VM_PAGE* new_page = vmpage_alloc(inode, offs, flags);
  vmpage_lock(new_page);

  // Hook the vm page to the inode
  INODE_LOCK(inode);
  LIST_FOREACH(&inode->i_pages, vmpage, struct VM_PAGE) {
    if (vmpage->vp_offset != offs)
      continue;

    // Page is already present - return the one already in use
    vmpage_lock(vmpage); // XXX is this order wise?
    INODE_UNLOCK(inode);

    // And throw the new page away, we won't need it
    vmpage_unlock(new_page);
    kfree(new_page);
    return vmpage;
  }

  // Not yet present; add the new page and return it
  LIST_APPEND(&inode->i_pages, new_page);
  INODE_UNLOCK(inode);
  return new_page;
}

struct VM_PAGE*
vmpage_create_private(int flags)
{
  auto new_page = vmpage_alloc(nullptr, 0, flags);

  // Hook a page to here as well, as the caller needs it anyway
  new_page->vp_page = page_alloc_single();
	KASSERT(new_page->vp_page != nullptr, "out of pages");
  return new_page;
}

void
vmpage_map(vmspace_t* vs, vmarea_t* va, struct VM_PAGE* vp)
{
	int flags = va->va_flags;
	struct PAGE* p = vmpage_get_page(vp);
	md_map_pages(vs, vp->vp_vaddr, page_get_paddr(p), 1, flags);
}

void
vmpage_zero(vmspace_t* vs, struct VM_PAGE* vp)
{
	// Clear the page XXX This is unfortunate, we should have a supply of pre-zeroed pages
	struct PAGE* p = vmpage_get_page(vp);
	md_map_pages(vs, vp->vp_vaddr, page_get_paddr(p), 1, VM_FLAG_READ | VM_FLAG_WRITE);
	memset((void*)vp->vp_vaddr, 0, PAGE_SIZE);
}

void vmpage_dump(struct VM_PAGE* vp, const char* prefix)
{
  kprintf("%s%p: refcount %d vaddr %p flags %s/%s/%s/%c ",
    prefix, vp, vp->vp_refcount,
    vp->vp_vaddr,
    (vp->vp_flags & VM_PAGE_FLAG_PRIVATE) ? "prv" : "pub",
    (vp->vp_flags & VM_PAGE_FLAG_READONLY) ? "ro" : "rw",
    (vp->vp_flags & VM_PAGE_FLAG_COW) ? "cow" : "---",
    (vp->vp_flags & VM_PAGE_FLAG_PENDING) ? 'p' : '.');
  if (vp->vp_flags & VM_PAGE_FLAG_LINK) {
    vp = vp->vp_link;
    kprintf(" -> ");
    vmpage_dump(vp, "");
    return;
  }
  if (vp->vp_page != nullptr)
    kprintf("%spage %p order %d", prefix, vp->vp_page, vp->vp_page->p_order);
  kprintf("\n");
}

/* vim:set ts=2 sw=2: */
