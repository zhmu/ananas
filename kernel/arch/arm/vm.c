#include <ananas/vm.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <machine/param.h>
#include <machine/vm.h>

uint32_t* vm_kernel_tt = NULL;

void
md_mapto(uint32_t* tt, addr_t va, addr_t pa, size_t num_pages, int flags)
{
	/* Figure out the access protection bits as accurately as we can */
	uint32_t l2_ap;
	if (flags & VM_FLAG_USER) {
		if (flags & VM_FLAG_WRITE)
			l2_ap = VM_AP_KRW_URW;
		else
			l2_ap = VM_AP_KRW_URO;
	} else {
		l2_ap = VM_AP_KRW_UNONE; /* XXX no support for kernel R/O (yet?) */
	}
	uint32_t l2_flags = VM_COARSE_SMALL_AP(l2_ap);
	if (flags & VM_FLAG_DEVICE)
		l2_flags |= VM_COARSE_DEVICE;

	while (num_pages > 0) {
		uint32_t* l1 = (uint32_t*)&tt[va >> 20]; /* note that typeof(tt) is uint32_t, so the 00 bits are added */
		if (*l1 == 0) {
			/* No mapping here yet; create a new one */
			void* ptr = kmalloc(VM_L2_TABLE_SIZE);
			memset(ptr, 0, VM_L2_TABLE_SIZE);
			*l1 = KVTOP((addr_t)ptr) | VM_TT_TYPE_COARSE; /* XXX domain=0 */
		}
		uint32_t* l2 = (uint32_t*)(KERNBASE + (*l1 & 0xfffffc00) + (((va & 0xff000) >> 12) << 2));
		*l2 = (pa & 0xfffff000) | l2_flags | VM_COARSE_TYPE_SMALL;
		va += PAGE_SIZE; pa += PAGE_SIZE; num_pages--;
	}
}

void*
vm_map_kernel(addr_t addr, size_t num_pages, int flags)
{
	md_mapto(vm_kernel_tt, PTOKV(addr), addr, num_pages, flags);
	return (void*)PTOKV(addr);
}

void
vm_unmap_kernel(addr_t addr, size_t num_pages)
{
	/* XXX */
}

void*
vm_map_device(addr_t addr, size_t len)
{
	panic("TODO: vm_map_device(): addr=%x, len=%x\n", addr, len);
	return NULL;
}

/* vim:set ts=2 sw=2: */
