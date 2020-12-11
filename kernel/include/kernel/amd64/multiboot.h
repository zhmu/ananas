/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>

/* As outlined in
 * https://www.gnu.org/software/grub/manual/multiboot/multiboot.html#Boot-information-format */
struct MULTIBOOT {
    /* 00 */ uint32_t mb_flags;
#define MULTIBOOT_FLAG_MEMORY (1 << 0)
#define MULTIBOOT_FLAG_BOOT_DEVICE (1 << 1)
#define MULTIBOOT_FLAG_CMDLINE (1 << 2)
#define MULTIBOOT_FLAG_MODULES (1 << 3)
#define MULTIBOOT_FLAG_SYM_AOUT (1 << 4)
#define MULTIBOOT_FLAG_SYM_ELF (1 << 5)
#define MULTIBOOT_FLAG_MMAP (1 << 6)
#define MULTIBOOT_FLAG_DRIVES (1 << 7)
#define MULTIBOOT_FLAG_CONFIG (1 << 8)
#define MULTIBOOT_FLAG_LOADER_NAME (1 << 9)
#define MULTIBOOT_FLAG_APM (1 << 10)
#define MULTIBOOT_FLAG_VBE (1 << 11)
    /* 04 */ uint32_t mb_mem_lower;
    /* 08 */ uint32_t mb_mem_higher;
    /* 12 */ uint32_t mb_boot_device;
    /* 16 */ uint32_t mb_cmdline;
    /* 20 */ uint32_t mb_mods_count;
    /* 24 */ uint32_t mb_mods_addr;
    /* 28 */ uint32_t mb_syms[4];
    /* 44 */ uint32_t mb_mmap_length;
    /* 48 */ uint32_t mb_mmap_addr;
    /* 52 */ uint32_t mb_drives_length;
    /* 56 */ uint32_t mb_drives_addr;
    /* 60 */ uint32_t mb_config_table;
    /* 64 */ uint32_t mb_boot_loader_name;
    /* 68 */ uint32_t mb_apm_table;
    /* 72 */ uint32_t mb_vbe_control_info;
    /* 76 */ uint32_t mb_vbe_mode_info;
    /* 80 */ uint16_t mb_vbe_mode;
    /* 82 */ uint16_t mb_vbe_interface_seg;
    /* 84 */ uint16_t mb_vbe_interface_off;
    /* 86 */ uint16_t mb_vbe_interface_len;
} __attribute__((packed));

struct MULTIBOOT_MMAP {
    uint32_t mm_entry_len;
    uint32_t mm_base_lo;
    uint32_t mm_base_hi;
    uint32_t mm_len_lo;
    uint32_t mm_len_hi;
    uint32_t mm_type;
#define MULTIBOOT_MMAP_AVAIL 1
} __attribute__((packed));
