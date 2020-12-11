/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>
#include "kernel-md/pcpu.h"

struct Thread;

/* Per-CPU information pointer */
struct PCPU {
    MD_PCPU_FIELDS      /* Machine-dependant data */
    uint32_t cpuid;     /* CPU ID */
    Thread* curthread;  /* current thread */
    Thread* idlethread; /* idle thread */
    int nested_irq;     /* number of nested IRQ functions */
};

/* Introduce a per-cpu structure */
void pcpu_init(struct PCPU* pcpu);
