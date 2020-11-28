/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __AMD64_MACRO_H__
#define __AMD64_MACRO_H__

#include "kernel-md/vm.h"

inline void SetIDTEntry(void* idt, const int num, const int type, const int ist, const void* handler)
{
    const auto target = reinterpret_cast<addr_t>(handler);
    uint8_t* p = reinterpret_cast<uint8_t*>(idt) + num * 16;

    // Target Offset 0:15
    p[0] = (((addr_t)handler)) & 0xff;
    p[1] = (((addr_t)handler) >> 8) & 0xff;
    // Target selector
    p[2] = GDT_SEL_KERNEL_CODE & 0xff;
    p[3] = (GDT_SEL_KERNEL_CODE >> 8) & 0xff;
    // IST 0:2
    p[4] = (ist);
    // Type 8:11, DPL 13:14, Present 15
    p[5] = type | (SEG_DPL_SUPERVISOR << 5) | (1 << 7);
    // Target offset 16:31
    p[6] = (target >> 16) & 0xff;
    p[7] = (target >> 24) & 0xff;
    // Target offset 32:63
    p[8] = (target >> 32) & 0xff;
    p[9] = (target >> 40) & 0xff;
    p[10] = (target >> 48) & 0xff;
    p[11] = (target >> 56) & 0xff;
    // Reserved
    p[12] = 0;
    p[13] = 0;
    p[14] = 0;
    p[15] = 0;
}

enum class GDTEntryType { Code, Data };

inline void SetGDTEntry(void* ptr, const int offs, const int dpl, const GDTEntryType type)
{
    auto p = (reinterpret_cast<uint8_t*>(ptr) + offs);
    // Segment Limit 0:15 (ignored)
    p[0] = 0;
    p[1] = 0;
    // Base Address 0:15 (ignored)
    p[2] = 0;
    p[3] = 0;
    // Base Address 16:23 (ignored)
    p[4] = 0;
    // Writable 9 [1], Conforming 10, Code 11, Must be set 12, DPL 13:14 (ignored), Present 15
    p[5] = (1 << 4) | (dpl << 5) | (1 << 7);
    if (type == GDTEntryType::Code)
        p[5] |= (1 << 3);
    if (type == GDTEntryType::Data)
        p[5] |= (1 << 1);
    // Segment limit 16:19, AVL 20, Long 21, D/B 22, Granulatity 23 (all ignored)
    p[6] = (1 << 5);
    // Base address 24:31 (ignored)
    p[7] = 0;
}

inline void SetGDTEntryTSS(void* ptr, const int offs, const int dpl, const addr_t base, const size_t size)
{
    auto p = reinterpret_cast<uint8_t*>(ptr) + offs;
    // Segment Limit 0:15
    p[0] = size & 0xff;
    p[1] = (size >> 8) & 0xff;
    // Base Address 0:15
    p[2] = base & 0xff;
    p[3] = (base >> 8) & 0xff;
    // Base Address 16:23
    p[4] = (base >> 16) & 0xff;
    // Type 8:11, DPL 13:14, Present 15
    p[5] = 9 | (dpl << 5) | (1 << 7);
    // Segment Limit 16:19, Available 20, Granularity 23
    p[6] = (size >> 16) & 0x7;
    // Base Address 24:31
    p[7] = (base >> 24) & 0xff;
    // Base Address 32:63
    p[8] = (base >> 32) & 0xff;
    p[9] = (base >> 40) & 0xff;
    p[10] = (base >> 48) & 0xff;
    p[11] = (base >> 56) & 0xff;
    // Reserved
    p[12] = 0;
    p[13] = 0;
    p[14] = 0;
    p[15] = 0;
}

template<typename Pointer> auto MakeRegister(const Pointer ptr, const size_t size)
{
    const auto addr = reinterpret_cast<addr_t>(ptr);
    struct Result { uint8_t val[10]; };
    return Result {{
        static_cast<uint8_t>((size - 1) & 0xff),
        static_cast<uint8_t>((size - 1) >> 8),
        static_cast<uint8_t>(addr & 0xff),
        static_cast<uint8_t>((addr >> 8) & 0xff),
        static_cast<uint8_t>((addr >> 16) & 0xff),
        static_cast<uint8_t>((addr >> 24) & 0xff),
        static_cast<uint8_t>((addr >> 32) & 0xff),
        static_cast<uint8_t>((addr >> 40) & 0xff),
        static_cast<uint8_t>((addr >> 48) & 0xff),
        static_cast<uint8_t>((addr >> 56) & 0xff)
    }};
}

static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t hi, lo;

    __asm __volatile("rdmsr\n" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

static inline void wrmsr(uint32_t msr, uint64_t val)
{
    __asm __volatile("wrmsr\n" : : "a"(val & 0xffffffff), "d"(val >> 32), "c"(msr));
}

static inline uint64_t read_cr0()
{
    uint64_t r;
    __asm __volatile("movq %%cr0, %0\n" : "=a"(r));
    return r;
}

static inline void write_cr0(uint64_t val) { __asm __volatile("movq %0, %%cr0\n" : : "a"(val)); }

static inline uint64_t read_cr4()
{
    uint64_t r;
    __asm __volatile("movq %%cr4, %0\n" : "=a"(r));
    return r;
}

static inline void write_cr4(uint64_t val) { __asm __volatile("movq %0, %%cr4\n" : : "a"(val)); }

#endif /* __AMD64_MACRO_H__ */
