#include <ananas/types.h>

#ifndef __VM_H__
#define __VM_H__

#define VM_FLAG_READ	0x0001
#define VM_FLAG_WRITE	0x0002
#define VM_FLAG_EXECUTE	0x0004
#define VM_FLAG_KERNEL	0x0008
#define VM_FLAG_USER	0x0010

/* Map a piece of memory */
void* vm_map_kernel(addr_t phys, size_t num_pages, int flags);

/* Unmap a piece of memory */
void vm_unmap_kernel(addr_t virt, size_t num_pages);

/* Map memory of a device so it can be accessed */
void* vm_map_device(addr_t phys, size_t len);

/* Initialize the memory manager */
void vm_init();

#endif /* __VM_H__ */
