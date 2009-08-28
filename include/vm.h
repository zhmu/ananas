#include "types.h"

#ifndef __VM_H__
#define __VM_H__

/* Map memory of a device so it can be accessed */
void* vm_map_device(addr_t addr, size_t len);

#endif /* __VM_H__ */
