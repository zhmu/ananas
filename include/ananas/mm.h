#ifndef __MM_H__
#define __MM_H__

#include <ananas/types.h>
#include <ananas/cdefs.h>

void* kmalloc(size_t len) __malloc;
void  kfree(void* ptr);

void mm_init();
void kmem_chunk_reserve(addr_t chunk_start, addr_t chunk_end, addr_t reserved_start, addr_t reserved_end, addr_t* out_start, addr_t* out_end);

#endif /* __MM_H__ */
