/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <cstdint>

struct BIO;

struct MBR_ENTRY {
    uint8_t bootable;
    uint8_t start_head;
    uint8_t start_sect_hi_cyl; /* bits 0-5: sector, 6-7: upper cylinder */
    uint8_t start_lo_cyl;      /* bits 0-5: sector, 6-7: upper cylinder */
    uint8_t system_id;
    uint8_t end_head;
    uint8_t end_sect_lo_cyl; /* bits 0-5: sector, 6-7: upper cylinder */
    uint8_t end_lo_cyl;      /* bits 0-5: sector, 6-7: upper cylinder */
    uint8_t lba_start[4];
    uint8_t lba_size[4];
} __attribute__((packed));

#define MBR_NUM_ENTRIES 4
#define MBR_SIGNATURE 0xaa55

struct MBR {
    uint8_t bootcode[446];
    struct MBR_ENTRY entry[MBR_NUM_ENTRIES];
    uint8_t signature[2];
} __attribute__((packed));

#ifdef KERNEL
class Device;

int mbr_process(Device* dev, struct BIO* bio);
#endif
