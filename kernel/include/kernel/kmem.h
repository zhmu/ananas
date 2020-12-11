/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>

/*
 * Per-mapping information structure.
 */
struct KMEM_MAPPING {
    addr_t kmm_virt; /* virtual address */
    addr_t kmm_phys; /* physical address */
    size_t kmm_size; /* size, in bytes */
    int kmm_flags;   /* flags */
};

void* kmem_map(addr_t phys, size_t length, int flags);
void kmem_unmap(void* virt, size_t length);
addr_t kmem_get_phys(void* virt);
