/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __ANANAS_SATA_H__
#define __ANANAS_SATA_H__

#include <ananas/types.h>

#define SATA_FIS_TYPE_H2D 0x27
#define SATA_FIS_UPD_C 0x80

struct Semaphore;

struct SATA_FIS_H2D {
    /* dw0 */
    uint8_t h2d_dw0_type; /* 7..0 */
    uint8_t h2d_dw0_upd;  /* 15..8 */
    uint8_t h2d_dw0_cmd;  /* 23..16 */
    uint8_t h2d_dw0_feat; /* 31..24 */
    /* dw1 */
    uint8_t h2d_dw1_sector;   /* 7..0 */
    uint8_t h2d_dw1_cyl_lo;   /* 15..8 */
    uint8_t h2d_dw1_cyl_hi;   /* 23..16 */
    uint8_t h2d_dw1_dev_head; /* 31..24 */
    /* dw2 */
    uint8_t h2d_dw2_sector_exp; /* 7..0 */
    uint8_t h2d_dw2_cyl_lo_exp; /* 15..8 */
    uint8_t h2d_dw2_cyl_hi_exp; /* 23..16 */
    uint8_t h2d_dw2_feat;       /* 31..24 */
    /* dw3 */
    uint8_t h2d_dw3_count;     /* 7..0 */
    uint8_t h2d_dw3_count_exp; /* 15..8 */
    uint8_t h2d_dw3_resvd0;    /* 23..16 */
    uint8_t h2d_dw3_control;   /* 31..24 */
    /* dw4 */
    uint8_t h2d_dw4_resvd3; /* 7..0 */
    uint8_t h2d_dw4_resvd2; /* 15..8 */
    uint8_t h2d_dw4_resvd1; /* 23..16 */
    uint8_t h2d_dw4_resvd0; /* 31..24 */
} __attribute__((packed));

struct SATA_REQUEST {
    union {
        struct SATA_FIS_H2D fis_h2d;
    } sr_fis;
    unsigned int sr_fis_length; /* FIS length, in bytes */
    uint16_t sr_count;          /* request length in bytes */
    void* sr_buffer;            /* data buffer, if not NULL */
    struct BIO* sr_bio;         /* associated I/O buffer, if not NULL */
    Semaphore* sr_semaphore;    /* Semaphore to signal on completion, if any */
    uint32_t sr_flags;
#define SATA_REQUEST_FLAG_READ (1 << 0)  /* Read request */
#define SATA_REQUEST_FLAG_WRITE (1 << 1) /* Write request */
#define SATA_REQUEST_FLAG_ATAPI (1 << 2) /* ATAPI request */
};

/* Signatures per device category */
#define SATA_SIG_ATA 0x0101
#define SATA_SIG_ATAPI 0xeb140101

void sata_fis_h2d_make_cmd(struct SATA_FIS_H2D* h2d, uint8_t cmd);
void sata_fis_h2d_make_cmd_lba48(
    struct SATA_FIS_H2D* h2d, uint8_t cmd, uint64_t lba, uint32_t count);

#endif /* __ANANAS_SATA_H__ */
