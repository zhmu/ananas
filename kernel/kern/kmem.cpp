/*
 * This code will handle kernel memory mappings. As the kernel has a limited
 * amount of virtual addresses (KVA), this code will handle the mapping of
 * physical addresses to KVA.
 *
 * We have several ranges at our disposal, each declared in kernel-md/vm.h:
 *
 * - KERNBASE .. ???: Kernel image, individual pieces may be mapped read-only,
 *   read-write, no-execute. The actual range is up to startup.c, which
 *   generally uses some linker-supplied variable to determine the length
 *   of the kernel.
 * - KMEM_DIRECT_PA_START .. KMEM_DIRECT_PA_END: This covers the physical
 *   addresses which can be mapped directly to a virtual address
 * - KMAP_DYNAMIC_VA_START .. KMAP_DYNAMIC_VA_END: The virtual addresses used
 *   for mappings which cannot be mapped directly because their physical
 *   adresses aren't in the KMEM_DIRECT_PA_START ... KMEM_DIRECT_PA_END range.
 *
 * The overal idea of this code is, when mapping physical address 'pa':
 *
 * - (1) KMEM_DIRECT_PA_START <= pa <= KMEM_DIRECT_PA_END can be mapped 1:1 to kernel
 *       virtual address 'va' where 'va = PA_TO_DIRECT_VA(pa)'
 * - (2) Addresses outside (1) are dynamically mapped using by finding an
 *       appropriate va which satisfies KMEM_DYNAMIC_VA_START <= va <=
 *       KMEM_DYNAMIC_VA_END
 *
 */
#include <machine/param.h>
#include "kernel/mm.h"
#include "kernel/kdb.h"
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/lock.h"
#include "kernel/page.h"
#include "kernel/vm.h"
#include "kernel-md/vm.h"
#include "options.h"

#define KMEM_DEBUG(...) (void)0

static Spinlock kmem_lock;
static Page* kmem_page;
static struct KMEM_MAPPING* kmem_mappings = NULL;

#define KMEM_NUM_MAPPINGS (PAGE_SIZE / sizeof(struct KMEM_MAPPING))

void*
kmem_map(addr_t phys, size_t length, int flags)
{
	KMEM_DEBUG("kmem_map(): phys=%p, len=%d, flags=%x\n", phys, length, flags);

	addr_t pa = phys & ~(PAGE_SIZE - 1);
	addr_t offset = phys & (PAGE_SIZE - 1);
	size_t size = (length + offset + PAGE_SIZE - 1) / PAGE_SIZE;

	/*
	 * First step is to see if we can directly map this; if this is the case,
	 * there is no need to allocate a specific mapping.
	 */
	if (pa >= KMEM_DIRECT_PA_START && pa < KMEM_DIRECT_PA_END &&
	   (flags & VM_FLAG_FORCEMAP) == 0) {
		addr_t va = PTOKV(pa);
		KMEM_DEBUG("kmem_map(): doing direct map: pa=%p va=%p size=%d\n", pa, va, size);
		md_kmap(pa, va, size, flags);
		return (void*)(va + offset);
	}

	if (kmem_mappings == NULL) {
		/*
		 * Chicken-and-egg problem here: we need a page to store our administration,
		 * but allocating one could invoke kmem_map() to allocate a mapping.
		 *
		 * XXX We just hope that this won't happen for now
		 */
		kmem_mappings = static_cast<struct KMEM_MAPPING*>(page_alloc_single_mapped(kmem_page, VM_FLAG_READ | VM_FLAG_WRITE));
		memset(kmem_mappings, 0, PAGE_SIZE);
	}

	/*
	 * Try to locate a sensible location for this mapping; we keep kmem_mappings sorted so we can
	 * easily reclaim virtual space in between.
	 */
	addr_t virt = KMEM_DYNAMIC_VA_START;
	{
		SpinlockGuard g(kmem_lock);
		struct KMEM_MAPPING* kmm = kmem_mappings;
		unsigned int n = 0;
		for (/* nothing */; n < KMEM_NUM_MAPPINGS; n++, kmm++) {
			if (kmm->kmm_size == 0)
				break; /* end of the line */
			if (virt + size <= kmm->kmm_virt)
				break; /* fits here */

			/*
			 * Only reject exact mappings; it's not up to us to determine how the
			 * caller should behave.
			 */
			if (pa == kmm->kmm_phys && size == kmm->kmm_size) {
				/* Already mapped */
				panic("XXX range %p-%p already mapped (%p-%p) -> %p",
				 pa, pa + size, kmm->kmm_phys, kmm->kmm_phys + size, kmm->kmm_virt);
				return NULL;
			}

			/*
			 * No fit here; try to place it after this mapping (this is an attempt to
			 * fill up holes)
			 */
			virt = kmm->kmm_virt + kmm->kmm_size * PAGE_SIZE;
		}

		if (virt + size >= KMEM_DYNAMIC_VA_END) {
			/* Out of KVA! XXX This shouldn't panic, but it's weird all-right */
			panic("out of kva (%p + %x >= %p)", virt, size, KMEM_DYNAMIC_VA_END);
			return NULL;
		}

		/*
		 * XXX I wonder how realistic this is and whether it's worth bothering to come up with
		 *     something more clever (there is no reason we couldn't move kmem_mappings)
		 */
		if (n == KMEM_NUM_MAPPINGS) {
			/* XXX This shouldn't panic either */
			panic("out of kva mappings!!");
			return NULL;
		}

		/*
		 * Need to insert the mapping at position n.
		 */
		memmove(&kmem_mappings[n + 1], &kmem_mappings[n], KMEM_NUM_MAPPINGS - n);
		kmm->kmm_phys = pa;
		kmm->kmm_size = size;
		kmm->kmm_flags = flags;
		kmm->kmm_virt = virt;
	}

	/* Now perform the actual mapping and we're set */
	KMEM_DEBUG(">>> DID outside kmem map: pa=%p virt=%p size=%d\n", pa, virt, size);

	md_kmap(pa, virt, size, flags);
	return (void*)(virt + offset);
}

void
kmem_unmap(void* virt, size_t length)
{
	addr_t va = (addr_t)virt & ~(PAGE_SIZE - 1);
	addr_t offset = (addr_t)virt & (PAGE_SIZE - 1);
	size_t size = (length + offset + PAGE_SIZE - 1) / PAGE_SIZE;
	KMEM_DEBUG("kmem_unmap(): virt=%p len=%d\n", virt, length);

	/* If this is a direct mapping, we can just as easily undo it */
	if (va >= PA_TO_DIRECT_VA(KMEM_DIRECT_PA_START) && va < PA_TO_DIRECT_VA(KMEM_DIRECT_PA_END)) {
		KMEM_DEBUG("kmem_unmap(): direct removed: virt=%p len=%d (range %p-%p)\n", virt, length,
		 PA_TO_DIRECT_VA(KMEM_DIRECT_PA_START), PA_TO_DIRECT_VA(KMEM_DIRECT_PA_END));

		md_kunmap(va, size);
		return;
	}

	/* We only allow exact mappings to be unmapped */
	kmem_lock.Lock();
	struct KMEM_MAPPING* kmm = kmem_mappings;
	for (unsigned int n = 0; n < KMEM_NUM_MAPPINGS; n++, kmm++) {
		if (kmm->kmm_size == 0)
			break; /* end of the line */
		if (va != kmm->kmm_virt || size != kmm->kmm_size)
			continue;

		KMEM_DEBUG("kmem_unmap(): unmapping: virt=%p len=%d\n", virt, length);

		memmove(&kmem_mappings[n], &kmem_mappings[n + 1], KMEM_NUM_MAPPINGS - n);
		/* And free the final one... */
		for (unsigned int m = KMEM_NUM_MAPPINGS - 1; m > n; m--) {
			struct KMEM_MAPPING* kmm = &kmem_mappings[m];
			if (kmm->kmm_size == 0)
				continue;
			memset(kmm, 0, sizeof(*kmm));
			break;
		}
		kmem_lock.Unlock();

		md_kunmap(va, size);
		return;
	}
	kmem_lock.Unlock();

	panic("kmem_unmap(): virt=%p length=%d not mapped", virt, length);
}

addr_t
kmem_get_phys(void* virt)
{
	addr_t va = (addr_t)virt & ~(PAGE_SIZE - 1);
	addr_t offset = (addr_t)virt & (PAGE_SIZE - 1);

	/* If this is a direct mapping, we needn't look it up at all */
	if (va >= PA_TO_DIRECT_VA(KMEM_DIRECT_PA_START) && va < PA_TO_DIRECT_VA(KMEM_DIRECT_PA_END))
		return (va - PA_TO_DIRECT_VA(KMEM_DIRECT_PA_START)) + offset;

	/* Walk through the mappings */
	{
		SpinlockGuard g(kmem_lock);
		struct KMEM_MAPPING* kmm = kmem_mappings;
		for (unsigned int n = 0; n < KMEM_NUM_MAPPINGS; n++, kmm++) {
			if (kmm->kmm_size == 0)
				break; /* end of the line */

			if (va < kmm->kmm_virt)
				continue;
			if (va > kmm->kmm_virt + kmm->kmm_size * PAGE_SIZE)
				continue;

			addr_t phys = kmm->kmm_phys + ((addr_t)virt - (addr_t)kmm->kmm_virt);
			return phys;
		}
	}

	panic("kmem_get_phys(): va=%p not mapped", va);
}

#ifdef OPTION_KDB
KDB_COMMAND(kmappings, NULL, "Display kernel memory mappings")
{
	if (kmem_mappings == NULL)
		return;

	struct KMEM_MAPPING* kmm = kmem_mappings;
	for (unsigned int n = 0; n < KMEM_NUM_MAPPINGS; n++, kmm++) {
		if (kmm->kmm_size == 0)
			break;
		size_t len = kmm->kmm_size * PAGE_SIZE;
		kprintf("mapping: va %p-%p pa %p-%p\n",
		 kmm->kmm_virt, kmm->kmm_virt + len - 1, kmm->kmm_phys, kmm->kmm_phys + len - 1);
	}
}
#endif

/* vim:set ts=2 sw=2: */
