#include <ananas/types.h>

#ifndef __VM_H__
#define __VM_H__

/* Map a piece of memory */
void vm_map(addr_t addr, size_t len);

/* Map a piece of memory to a specific location */
void vm_mapto(addr_t virt, addr_t phys, size_t num_pages);

/* Unmap a piece of memory */
void vm_unmap(addr_t addr, size_t len);

/* Map memory of a device so it can be accessed */
void* vm_map_device(addr_t addr, size_t len);

/* Initialize the memory manager */
void vm_init();

#endif /* __VM_H__ */
