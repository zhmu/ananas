#include "types.h"

#ifndef __VM_H__
#define __VM_H__

/* Map a piece of memory */
void vm_map(addr_t addr, size_t len);

/* Map memory of a device so it can be accessed */
void* vm_map_device(addr_t addr, size_t len);

/* Initialize the memory manager */
void vm_init();

#endif /* __VM_H__ */
