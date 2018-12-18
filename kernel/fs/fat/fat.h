#include <ananas/types.h>

#ifndef __FAT_H__
#define __FAT_H__

struct FAT16_EPB {
    uint8_t epb_drivenum;  /* drive number */
    uint8_t epb_nt_flags;  /* flags (NT only) */
    uint8_t epb_signature; /* signature */
#define FAT16_EPB_SIGNATURE1 0x28
#define FAT16_EPB_SIGNATURE2 0x29
    uint8_t epb_serial[4];         /* disk serial number */
    uint8_t epb_label[11];         /* volume label */
    uint8_t epb_systemid[8];       /* system identifier */
    uint8_t epb_bootcode[448];     /* boot code */
    uint8_t epb_boot_signature[2]; /* signature */
#define EPB_BOOT_SIGNATURE 0xaa55
} __attribute__((packed));

struct FAT32_EPB {
    uint8_t epb_sectors_per_fat[4];    /* Size of FAT in sectors */
    uint8_t epb_flags[2];              /* flags */
    uint8_t epb_fat_version[2];        /* FAT version */
    uint8_t epb_root_cluster[4];       /* root directory cluster */
    uint8_t epb_fsinfo_sector[2];      /* fsinfo sector number */
    uint8_t epb_backup_boot_sector[2]; /* backup boot sector */
    uint8_t epb_reserved[12];          /* reserved */
    uint8_t epb_drivenum;              /* drive number */
    uint8_t epb_nt_flags;              /* flags (NT only) */
    uint8_t epb_signature;             /* signature */
    uint8_t epb_serial[4];             /* disk serial number */
    uint8_t epb_label[11];             /* volume label */
    uint8_t epb_systemid[8];           /* system identifier */
    uint8_t epb_bootcode[420];         /* boot code */
    uint8_t epb_boot_signature[2];     /* signature */
} __attribute__((packed));

struct FAT_ENTRY {
    uint8_t fe_filename[11]; /* famous 8.11 file name */
    uint8_t fe_attributes;   /* attribute bits */
#define FAT_ATTRIBUTE_READONLY 0x01
#define FAT_ATTRIBUTE_HIDDEN 0x02
#define FAT_ATTRIBUTE_SYSTEM 0x04
#define FAT_ATTRIBUTE_VOLUMEID 0x08
#define FAT_ATTRIBUTE_DIRECTORY 0x10
#define FAT_ATTRIBUTE_ARCHIVE 0x20
#define FAT_ATTRIBUTE_LFN \
    (FAT_ATTRIBUTE_VOLUMEID | FAT_ATTRIBUTE_SYSTEM | FAT_ATTRIBUTE_HIDDEN | FAT_ATTRIBUTE_READONLY)
    uint8_t fe_reserved;       /* reserved (NT) */
    uint8_t fe_create_timesec; /* creation time (10th of seconds) */
    uint8_t fe_create_time[2]; /* creation time (sec: 8; hour: 5, min: 6, 5: sec) */
    uint8_t fe_create_date[2]; /* creation date (year: 7, month: 4, day: 5) */
    uint8_t fe_access_date[2]; /* access date */
    uint8_t fe_cluster_hi[2];  /* cluster number (hi 16 bits) */
    uint8_t fe_modify_time[2]; /* creation time */
    uint8_t fe_modify_date[2]; /* creation date */
    uint8_t fe_cluster_lo[2];  /* cluster number (lo 16 bits) */
    uint8_t fe_size[4];        /* file size */
} __attribute__((packed));

struct FAT_ENTRY_LFN {
    uint8_t lfn_order; /* order number */
#define LFN_ORDER_LAST 0x40
    uint8_t lfn_name_1[10]; /* first 5x 16bit filename */
    uint8_t lfn_attribute;  /* attribute (must be FAT_ATTRIBUTE_LFN) */
    uint8_t lfn_type;       /* long entry type */
    uint8_t lfn_checksum;   /* checksum */
    uint8_t lfn_name_2[12]; /* second 5x 16bit filename */
    uint8_t lfn_zero[2];    /* always zero */
    uint8_t lfn_name_3[4];  /* final 2x 16bit filename */
} __attribute__((packed));

struct FAT_BPB {
    uint8_t bpb_jmp[3];               /* jmp short bootcode */
    uint8_t bpb_oem[8];               /* OEM identifier */
    uint8_t bpb_bytespersector[2];    /* bytes per sector */
    uint8_t bpb_sectorspercluster;    /* sectors per cluster */
    uint8_t bpb_num_reserved[2];      /* number of reserved sectors */
    uint8_t bpb_num_fats;             /* number of FATs on disk */
    uint8_t bpb_num_root_entries[2];  /* number of root directory entries */
    uint8_t bpb_num_sectors[2];       /* number of total sectors (if <65k) */
    uint8_t bpb_media_type;           /* media type */
    uint8_t bpb_sectors_per_fat[2];   /* number of sectors per FAT */
    uint8_t bpb_sectors_per_track[2]; /* number of sectors per track */
    uint8_t bpb_num_heads[2];         /* number of heads */
    uint8_t bpb_hidden_sectors[4];    /* number of hidden sectors */
    uint8_t bpb_large_num_sectors[4]; /* number of total sectors (if >=65k) */
    union {
        struct FAT16_EPB fat16;
        struct FAT32_EPB fat32;
    } epb;
} __attribute__((packed));

struct FAT_FAT32_FSINFO {
    uint8_t fsi_signature1[4]; /* FSINFO signature */
#define FAT_FSINFO_SIGNATURE1 0x41615252
    uint8_t fsi_reserved1[480]; /* Reserved */
    uint8_t fsi_signature2[4];  /* Another signature */
#define FAT_FSINFO_SIGNATURE2 0x61417272
    uint8_t fsi_free_count[4]; /* Free cluster count*/
    uint8_t fsi_next_free[4];  /* Next free cluster */
    uint8_t fsi_reserved2[12]; /* Reserved */
} __attribute__((packed));

#endif /* __FAT_H__ */
