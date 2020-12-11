/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <sys/types.h>

static inline void outb(uint16_t port, uint8_t data)
{
    __asm volatile("outb %0, %w1" : : "a"(data), "d"(port));
}

static inline void outw(uint16_t port, uint16_t data)
{
    __asm volatile("outw %0, %w1" : : "a"(data), "d"(port));
}

static inline void outl(uint16_t port, uint32_t data)
{
    __asm volatile("outl %0, %w1" : : "a"(data), "d"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t a;
    __asm volatile("inb %w1, %0" : "=a"(a) : "d"(port));
    return a;
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t a;
    __asm volatile("inw %w1, %0" : "=a"(a) : "d"(port));
    return a;
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t a;
    __asm volatile("inl %w1, %0" : "=a"(a) : "d"(port));
    return a;
}

static inline uint64_t rdtsc()
{
    uint64_t v;
    __asm __volatile("rdtsc\n"
                     "shlq $32, %%rdx\n"
                     "orq %%rdx, %0\n"
                     : "=a"(v)
                     :
                     : "rdx");
    return v;
}
