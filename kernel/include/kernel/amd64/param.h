/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <machine/param.h>

/* amd64 is little endian */
#define LITTLE_ENDIAN

/* amd64 is 64 bit */
#define ARCH_BITS 64

/* This is the base address where the kernel should be linked to */
#define KERNBASE 0xffffffff80000000

/* Temporary mapping address for userland - will shift per CPU */
#define TEMP_USERLAND_ADDR 0xfffffffffff00000

/* Virtual address of the per-thread stack */
#define USERLAND_STACK_ADDR 0x80000

/* Virtual address of the shared userland support page */
#define USERLAND_SUPPORT_ADDR 0x7e000

/* Temporary mapping address size */
#define TEMP_USERLAND_SIZE PAGE_SIZE

/* Number of Global Descriptor Table entries */
#define GDT_NUM_ENTRIES 9

/* Number of Interrupt Descriptor Table entries */
#define IDT_NUM_ENTRIES 256

/* Kernel stack size */
#define KERNEL_STACK_SIZE 0x4000

/* Thread stack size */
#define THREAD_STACK_SIZE 0x8000

/* First thread mapping virtual address */
#define THREAD_INITIAL_MAPPING_ADDR 10485760
