/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#define SMP_IPI_FIRST 0xf0
#define SMP_IPI_COUNT 4
#define SMP_IPI_PANIC 0xf0    /* IPI used to trigger panic situation on other CPU's */
#define SMP_IPI_PERIODIC 0xf1 /* periodic cpu-local timer interrupt */

#ifndef ASM

struct X86_CPU {
    int cpu_lapic_id = -1;     /* Local APIC ID */
    char* cpu_stack = nullptr; /* CPU stack */
    void* cpu_pcpu = nullptr;  /* PCPU pointer */
    char* cpu_gdt = nullptr;   /* Global Descriptor Table */
    char* cpu_tss = nullptr;   /* Task State Segment */
};

struct PCPU;

namespace smp
{
    void Init(struct PCPU&);
    void Prepare();
    void PanicOthers();
    void InitTimer();

} // namespace smp
#endif
