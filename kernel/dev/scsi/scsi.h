/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANAS_SCSI_H
#define ANANAS_SCSI_H

#define SCSI_CMD_TEST_UNIT_READY 0x00
#define SCSI_CMD_REQUEST_SENSE 0x3
#define SCSI_CMD_INQUIRY_6 0x12
#define SCSI_CMD_READ_CAPACITY_10 0x25
#define SCSI_CMD_READ_10 0x28

struct SCSI_CDB_6 {
    /* 00 */ uint8_t c_code;
    /* 01 */ uint8_t c_misc;
    /* 02-03 */ uint16_t c_lba;
    /* 04 */ uint8_t c_alloc_len;
    /* 05 */ uint8_t c_control;
} __attribute__((packed));

struct SCSI_CDB_10 {
    /* 00 */ uint8_t c_code;
    /* 01 */ uint8_t c_misc1;
    /* 02-05 */ uint32_t c_lba;
    /* 06 */ uint8_t c_misc2;
    /* 07-08 */ uint16_t c_alloc_len;
    /* 09 */ uint8_t c_control;
} __attribute__((packed));

struct SCSI_TEST_UNIT_READY_CMD {
    /* 00 */ uint8_t c_code;
    /* 01-04 */ uint8_t c_reserved[4];
    /* 05 */ uint8_t c_control;
} __attribute__((packed));

struct SCSI_REQUEST_SENSE_CMD {
    /* 00 */ uint8_t c_code;
    /* 01 */ uint8_t c_desc;
    /* 02-03 */ uint8_t c_reserved[2];
    /* 04 */ uint8_t c_alloc_len;
    /* 05 */ uint8_t c_control;
} __attribute__((packed));

struct SCSI_FIXED_SENSE_DATA {
    /* 00 */ uint8_t sd_code;
#define SCSI_FIXED_SENSE_CODE_VALID (1 << 7)
#define SCSI_FIXED_SENSE_CODE_CODE(x) ((x)&0x7f)
    /* 01 */ uint8_t sd_obsolete;
    /* 02 */ uint8_t sd_flags;
#define SCSI_FIXED_SENSE_FLAGS_FILEMARK (1 << 7)
#define SCSI_FIXED_SENSE_FLAGS_EOM (1 << 6)
#define SCSI_FIXED_SENSE_FLAGS_ILI (1 << 5)
#define SCSI_FIXED_SENSE_FLAGS_KEY(x) ((x)&0xf)
    /* 03-06 */ uint8_t sd_info[4];
    /* 07 */ uint8_t sd_add_length;
    /* 08-0b */ uint8_t sd_cmd_info[4];
    /* 0c */ uint8_t sd_add_sense_code;
    /* 0d */ uint8_t sd_add_sense_qualifier;
    /* 0e */ uint8_t sd_field_replace_code;
    /* 0f-11 */ uint8_t sd_sense_key_specific[3];
    /* 13-.. */ uint8_t sd_add_sense_bytes[0];
} __attribute__((packed));

struct SCSI_READ_10_CMD {
    /* 00 */ uint8_t c_code;
    /* 01 */ uint8_t c_flags;
#define SCSI_READ_10_FLAG_DPO (1 << 4)
#define SCSI_READ_10_FLAG_FUA (1 << 3)
#define SCSI_READ_10_FLAG_FUA_NV (1 << 1)
    /* 02-05 */ uint32_t c_lba;
    /* 06 */ uint8_t c_group_number;
    /* 07-08 */ uint16_t c_transfer_len;
    /* 09 */ uint8_t c_control;
} __attribute__((packed));

struct SCSI_INQUIRY_6_CMD {
    /* 00 */ uint8_t c_code;
    /* 01 */ uint8_t c_evpd;
    /* 02 */ uint8_t c_page_code;
    /* 03-04 */ uint16_t c_alloc_len;
    /* 05 */ uint8_t c_control;
} __attribute__((packed));

struct SCSI_INQUIRY_6_REPLY {
    /* 00 */ uint8_t r_peripheral;
    /* 01 */ uint8_t r_flags1;
#define SCSI_INQUIRY_6_FLAG1_RMB (1 << 7)
    /* 02 */ uint8_t r_version;
    /* 03 */ uint8_t r_flags3;
    /* 04 */ uint8_t r_add_length;
    /* 05 */ uint8_t r_flags5;
    /* 06 */ uint8_t r_flags6;
    /* 07 */ uint8_t r_flags7;
    /* 08-0f */ uint8_t r_vendor_id[8];
    /* 10-1f */ uint8_t r_product_id[16];
    /* 20-23 */ uint8_t r_revision[4];
} __attribute__((packed));

struct SCSI_READ_CAPACITY_10_CMD {
    /* 00 */ uint8_t c_code;
    /* 01 */ uint8_t c_reserved1;
    /* 02-05 */ uint32_t c_lba;
    /* 06 */ uint8_t c_reserved2;
    /* 07 */ uint8_t c_reserved3;
    /* 08 */ uint8_t c_pmi;
    /* 09 */ uint8_t c_control;
} __attribute__((packed));

struct SCSI_READ_CAPACITY_10_REPLY {
    /* 00-03 */ uint32_t r_lba;
    /* 04-07 */ uint32_t r_block_length;
} __attribute__((packed));

#endif // ANANAS_SCSI_H
