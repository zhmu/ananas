/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/lib.h"
#include "kernel/pcpu.h"
#include "kernel/result.h"
#include "kernel/thread.h"

void pcpu_init(struct PCPU* pcpu)
{
    char name[64];
    snprintf(name, sizeof(name) - 1, "idle:cpu%u", pcpu->cpuid);
    name[sizeof(name) - 1] = '\0';
    if (auto result = kthread_alloc(name, &idle_thread, NULL, pcpu->idlethread); result.IsFailure())
        panic("cannot create idlethread");
    pcpu->nested_irq = 0;

    /*
     * Hook the idle thread to its specific CPU and set the appropriate priority;
     * it must only be run as a last-resort.
     */
    pcpu->idlethread->t_affinity = pcpu->cpuid;
    pcpu->idlethread->t_priority = THREAD_PRIORITY_IDLE;
}
