#include <ananas/types.h>

#ifndef __VM_H__
#define __VM_H__

#define VM_FLAG_READ	0x0001
#define VM_FLAG_WRITE	0x0002
#define VM_FLAG_EXECUTE	0x0004
#define VM_FLAG_KERNEL	0x0008
#define VM_FLAG_USER	0x0010
#define VM_FLAG_DEVICE	0x0020

/* Force a specific mapping to be made */
#define VM_FLAG_FORCEMAP	0x0040

#if 0
/* Map a piece of memory */
void* vm_map_kernel(addr_t phys, size_t num_pages, int flags);

/* Unmap a piece of memory */
void vm_unmap_kernel(addr_t virt, size_t num_pages);

/* Map memory of a device so it can be accessed */
void* vm_map_device(addr_t phys, size_t len);
#endif

/* Maps a piece of memory for kernel use */
void md_kmap(addr_t phys, addr_t virt, size_t num_pages, int flags);

/* Unmaps a piece of kernel memory */
void md_kunmap(addr_t virt, size_t num_pages);

/* Initialize the memory manager */
void vm_init();

#endif /* __VM_H__ */
