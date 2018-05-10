#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/vmarea.h"
#include "kernel/vmspace.h"
#include "kernel/vmpage.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vm.h"

TRACE_SETUP;

namespace {

Result
read_data(DEntry& dentry, void* buf, off_t offset, size_t len)
{
	struct VFS_FILE f;
	memset(&f, 0, sizeof(f));
	f.f_dentry = &dentry;

	RESULT_PROPAGATE_FAILURE(
		vfs_seek(&f, offset)
	);

	size_t amount = len;
	RESULT_PROPAGATE_FAILURE(
		vfs_read(&f, buf, &amount)
	);

	if (amount != len)
		return RESULT_MAKE_FAILURE(ERANGE);
	return Result::Success();
}

int
vmspace_page_flags_from_va(const VMArea& va)
{
	int flags = 0;
	if ((va.va_flags & (VM_FLAG_READ | VM_FLAG_WRITE)) == VM_FLAG_READ) {
		flags |= VM_PAGE_FLAG_READONLY;
	}
	return flags;
}

VMPage&
vmspace_get_dentry_backed_page(VMArea& va, off_t read_off)
{
	// First, try to lookup the page; if we already have it, no need to read it
	VMPage* vmpage = vmpage_lookup_locked(va, *va.va_dentry->d_inode, read_off);
	if (vmpage == nullptr) {
		// Page not found - we need to allocate one. This is always a shared mapping, which we'll copy if needed
		vmpage = &vmpage_create_shared(*va.va_dentry->d_inode, read_off, VM_PAGE_FLAG_PENDING | vmspace_page_flags_from_va(va));
	}
	// vmpage will be locked at this point!

	if ((vmpage->vp_flags & VM_PAGE_FLAG_PENDING) == 0)
		return *vmpage;

	// Read the page - note that we hold the vmpage lock while doing this
	Page* p;
	void* page = page_alloc_single_mapped(p, VM_FLAG_READ | VM_FLAG_WRITE);
	KASSERT(p != nullptr, "out of memory"); // XXX handle this

	size_t read_length = PAGE_SIZE;
	if (read_off + read_length > va.va_dentry->d_inode->i_sb.st_size) {
		// This inode is simply not long enough to cover our read - adjust XXX what when it grows?
		read_length = va.va_dentry->d_inode->i_sb.st_size - read_off;
		// Zero out everything after the part we will read so we don't leak any data
		memset(static_cast<char*>(page) + read_length, 0, PAGE_SIZE - read_length);
	}

	Result result = read_data(*va.va_dentry, page, read_off, read_length);
	kmem_unmap(page, PAGE_SIZE);
	KASSERT(result.IsSuccess(), "cannot deal with error %d", result.AsStatusCode()); // XXX

	// Update the vm page to contain our new address
	vmpage->vp_page = p;
	vmpage->vp_flags &= ~VM_PAGE_FLAG_PENDING;
	return *vmpage;
}

} // unnamed namespace

Result
VMSpace::HandleFault(addr_t virt, int flags)
{
	TRACE(VM, INFO, "HandleFault(): vs=%p, virt=%p, flags=0x%x", this, virt, flags);
	//kprintf("HandleFault(): vs=%p, virt=%p, flags=0x%x\n", vs, virt, flags);

	// Walk through the areas one by one
	for(auto& va: vs_areas) {
		if (!(virt >= va.va_virt && (virt < (va.va_virt + va.va_len))))
			continue;

		/* We should only get faults for lazy areas (filled by a function) or when we have to dynamically allocate things */
		KASSERT((va.va_flags & VM_FLAG_FAULT) != 0, "unexpected pagefault in area %p, virt=%p, len=%d, flags 0x%x", &va, va.va_virt, va.va_len, va.va_flags);

		// See if we have this page mapped
		VMPage* vp = va.LookupVAddrAndLock(virt & ~(PAGE_SIZE - 1));
		if (vp != nullptr) {
			if ((flags & VM_FLAG_WRITE) && (vp->vp_flags & VM_PAGE_FLAG_COW)) {
				// Promote our copy to a writable page and update the mapping
				KASSERT((vp->vp_flags & VM_PAGE_FLAG_READONLY) == 0, "cowing r/o page");
				vp = &va.PromotePage(*vp);
				vp->Map(*this, va);
				vp->Unlock();
				return Result::Success();
			}

			// Page is already mapped, but not COW. Bad, reject
			vp->Unlock();
			return RESULT_MAKE_FAILURE(EFAULT);
		}

		// XXX we expect va_doffset to be page-aligned here (i.e. we can always use a page directly)
		// this needs to be enforced when making mappings!
		KASSERT((va.va_doffset & (PAGE_SIZE - 1)) == 0, "doffset %x not page-aligned", (int)va.va_doffset);

		// If there is a dentry attached here, perhaps we may find what we need in the corresponding inode
		if (va.va_dentry != nullptr) {
			/*
			 * The way dentries are mapped to virtual address is:
			 *
			 * 0       va_doffset                               file length
			 * +------------+-------------+-------------------------------+
			 * |            |XXXXXXXXXXXXX|                               |
			 * |            |XXXXXXXXXXXXX|                               |
			 * +------------+-------------+-------------------------------+
			 *             /     |||      \ va_doffset + va_dlength
			 *            /      vvv
			 *     +-------------+---------------+
			 *     |XXXXXXXXXXXXX|000000000000000|
			 *     |XXXXXXXXXXXXX|000000000000000|
			 *     +-------------+---------------+
			 *     0            \
			 *                   \
			 *                    va_dlength
			 */
			off_t read_off = (virt & ~(PAGE_SIZE - 1)) - va.va_virt; // offset in area, still needs va_doffset added
			if (read_off < va.va_dlength) {
				// At least (part of) the page is to be read from the backing dentry -
				// this means we want the entire page
				VMPage& vmpage = vmspace_get_dentry_backed_page(va, read_off + va.va_doffset);
				// vmpage is locked at this point

				// If the mapping is page-aligned and read-only or shared, we can re-use the
				// mapping and avoid the entire copy
				VMPage* new_vp;
				bool can_reuse_page_1on1 = true;
				// Reusing means the page resides in the section...
				can_reuse_page_1on1 &= (read_off + PAGE_SIZE) <= va.va_dlength;
				// ... and we have a page-aligned offset
				can_reuse_page_1on1 &= (va.va_doffset & (PAGE_SIZE - 1)) == 0;
				if (can_reuse_page_1on1) {
					// Just clone the page - note that we do not have a source vmspace as the page
					// came from the inode (XXX CHECK THIS!)
					new_vp = &vmpage_clone(nullptr, *this, va, va, vmpage);
					new_vp->vp_vaddr = virt & ~(PAGE_SIZE - 1);
				} else {
					// Cannot re-use; create a new VM page, with appropriate flags based on the va
					new_vp = &va.AllocatePrivatePage(virt & ~(PAGE_SIZE - 1), VM_PAGE_FLAG_PRIVATE | vmspace_page_flags_from_va(va));

					// Now copy the parts of the dentry-backed page
					size_t copy_len = va.va_dlength - read_off; // this is size-left after where we read
					if (copy_len > PAGE_SIZE)
						copy_len = PAGE_SIZE;
					vmpage.CopyExtended(*new_vp, copy_len);
				}
				vmpage.Unlock();

				// Finally, update the permissions and we are done
				new_vp->Map(*this, va);
				new_vp->Unlock();
				return Result::Success();
			}
		}

		// We need a new VM page here; this is an anonymous mapping which we need to back
		VMPage& new_vp = va.AllocatePrivatePage(virt & ~(PAGE_SIZE - 1), VM_PAGE_FLAG_PRIVATE);

		// Ensure the page cleaned so we don't leak any information
		new_vp.Zero(*this);

		// And now (re)map the page for the caller
		new_vp.Map(*this, va);
		new_vp.Unlock();
		return Result::Success();
	}

	return RESULT_MAKE_FAILURE(EFAULT);
}

/* vim:set ts=2 sw=2: */
