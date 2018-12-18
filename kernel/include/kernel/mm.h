/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __MM_H__
#define __MM_H__

#include <ananas/types.h>
#include "kernel/cdefs.h"

#ifdef __cplusplus
extern "C" {
#endif
void* kmalloc(size_t len) __malloc;
void kfree(void* ptr);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
void* operator new(size_t len);
void operator delete(void* p) noexcept;

inline void* operator new(size_t len, void* p) noexcept { return p; }
inline void* operator new[](size_t len, void* p) noexcept { return p; }
inline void operator delete(void*, void*)noexcept {}
inline void operator delete[](void*, void*) noexcept {}
#endif

void kmem_chunk_reserve(
    addr_t chunk_start, addr_t chunk_end, addr_t reserved_start, addr_t reserved_end,
    addr_t* out_start, addr_t* out_end);

#endif /* __MM_H__ */
