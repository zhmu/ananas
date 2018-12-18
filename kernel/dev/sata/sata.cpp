/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/dev/sata.h"
#include "kernel/lib.h"

void sata_fis_h2d_make_cmd(struct SATA_FIS_H2D* h2d, uint8_t cmd)
{
    memset(h2d, 0, sizeof(*h2d));
    h2d->h2d_dw0_type = SATA_FIS_TYPE_H2D;
    h2d->h2d_dw0_cmd = cmd;
    h2d->h2d_dw0_upd = SATA_FIS_UPD_C;
}

void sata_fis_h2d_make_cmd_lba48(
    struct SATA_FIS_H2D* h2d, uint8_t cmd, uint64_t lba, uint32_t count)
{
    sata_fis_h2d_make_cmd(h2d, cmd);
    h2d->h2d_dw3_count_exp = (count >> 8) & 0xff;
    h2d->h2d_dw3_count = count & 0xff;
    h2d->h2d_dw1_dev_head = (1 << 6) /* LBA addr */ | (lba >> 24);

    h2d->h2d_dw1_sector = lba & 0xff;
    h2d->h2d_dw1_cyl_lo = (lba >> 8) & 0xff;
    h2d->h2d_dw1_cyl_hi = (lba >> 16) & 0xff;
    h2d->h2d_dw2_sector_exp = (lba >> 24) & 0xff;
    h2d->h2d_dw2_cyl_lo_exp = (lba >> 32) & 0xff;
    h2d->h2d_dw2_cyl_hi_exp = (lba >> 40) & 0xff;
}
