/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>

#ifndef __EXT2_H__
#define __EXT2_H__

struct EXT2_SUPERBLOCK {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
#define EXT2_SUPER_MAGIC 0xef53
    uint16_t s_state;
#define EXT2_VALID_FS 1
#define EXT2_ERROR_FS 2
    uint16_t s_errors;
#define EXT2_ERRORS_CONTINUE 1
#define EXT2_ERRORS_RO 2
#define EXT2_ERRORS_PANIC 3
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
#define EXT2_OS_LINUX 0
#define EXT2_OS_HURD 1
#define EXT2_OS_MASIX 2
#define EXT2_OS_FREEBSD 3
#define EXT2_OS_LITES 4
    uint32_t s_rev_level;
#define EXT2_GOOD_OLD_REV 0
#define EXT2_DYNAMIC_REV 1
    uint16_t s_def_resuid;
#define EXT2_DEF_RESUID 0
    uint16_t s_def_resgid;
#define EXT2_DEF_RESGID 0
    /* Note: next fields do not exist in the good old revision */
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
#define EXT2_FEATURE_COMPAT_DIR_PREALLIC 0x0001
#define EXT2_FEATURE_COMPAT_IMAGIC_INPODES 0x0002
#define EXT2_FEATURE_COMPAT_HAS_JOURNAL 0x0004
#define EXT2_FEATURE_COMPAT_EXT_ATTR 0x0008
#define EXT2_FEATURE_COMPAT_RESIZE_INO 0x0010
#define EXT2_FEATURE_COMPAT_DIR_INDEX 0x0020
    uint32_t s_feature_incompat;
#define EXT2_FEATURE_INCOMPAT_COMPRESSION 0x0001
#define EXT2_FEATURE_INCOMPAT_FILETYPE 0x0002
#define EXT2_FEATURE_INCOMPAT_RECOVER 0x0004
#define EXT2_FEATURE_INCOMPAT_JOURNAL_DEV 0x0008
#define EXT2_FEATURE_INCOMPAT_META_BG 0x0010
    uint32_t s_feature_ro_compat;
#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER 0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE 0x0002
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR 0x0004
    uint8_t s_uuid[16];
    uint8_t s_volume_name[16];
    uint8_t s_last_mounted[64];
    uint32_t s_algo_bitmap;
#define EXT2_LZV1_ALG 0x0001
#define EXT2_LZRW3A_ALG 0x0002
#define EXT2_GZIP_ALG 0x0004
#define EXT2_BZIP2_ALG 0x0008
#define EXT2_LZO_ALG 0x0010
    uint8_t s_prealloc_blocks;
    uint8_t s_prealloc_dir_blocks;
    uint16_t _padding0;
    uint8_t s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t s_def_hash_version;
    uint8_t _padding1[3];
    uint32_t s_default_mount_options;
    uint32_t s_first_meta_bg;
    uint8_t _reserved[760];
} __attribute__((packed));

struct EXT2_BLOCKGROUP {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint8_t bg_reserved[12];
} __attribute__((packed));

struct EXT2_INODE {
    uint16_t i_mode;
#define EXT2_S_IFSOCK 0xc000
#define EXT2_S_IFLNK 0xa000
#define EXT2_S_IFREG 0x8000
#define EXT2_S_IFBLK 0x6000
#define EXT2_S_IFDIR 0x4000
#define EXT2_S_IFCHR 0x2000
#define EXT2_S_IFIFO 0x1000
#define EXT2_S_ISUID 0x0800
#define EXT2_S_ISGID 0x0400
#define EXT2_S_ISVTX 0x0200
#define EXT2_S_IRUSR 0x0100
#define EXT2_S_IWUSR 0x0080
#define EXT2_S_IXUSR 0x0040
#define EXT2_S_IRGRP 0x0020
#define EXT2_S_IWGRP 0x0010
#define EXT2_S_IXGRP 0x0008
#define EXT2_S_IROTH 0x0004
#define EXT2_S_IWOTH 0x0002
#define EXT2_S_IXOTH 0x0001
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;
    uint32_t i_flags;
#define EXT2_SECRM_FL 0x00000001
#define EXT2_UNRM_FL 0x00000002
#define EXT2_COMPR_FL 0x00000004
#define EXT2_SYNC_FL 0x00000008
#define EXT2_IMMUTABLE_FL 0x00000010
#define EXT2_APPEND_FL 0x00000020
#define EXT2_NODUMP_FL 0x00000040
#define EXT2_NOATIME_FL 0x00000080
#define EXT2_DIRTY_FL 0x00000100
#define EXT2_COMPRBLK_FL 0x00000200
#define EXT2_NOCOMPR_FL 0x00000400
#define EXT2_ECOMPR_FL 0x00000800
#define EXT2_BTREE_FL 0x00010000
#define EXT2_INDEX_FL 0x00010000 /* ? */
#define EXT2_IMAGIC_FL 0x00020000
#define EXT2_JOURNAL_DATA_FL 0x00040000
#define EXT2_RESERVED_FL 0x80000000

    uint32_t i_osd1;
#define EXT2_INODE_BLOCKS 15
    uint32_t i_block[EXT2_INODE_BLOCKS];
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint8_t i_osd2[12];
} __attribute__((packed));

#define EXT2_BAD_INO 1
#define EXT2_ROOT_INO 2
#define EXT2_ACL_IDX_INO 3
#define EXT2_ACL_DATA_INO 4
#define EXT2_BOOT_LOADER_INO 5
#define EXT2_UNDEL_DIR_INO 6

struct EXT2_DIRENTRY {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
#define EXT2_FT_UNKNOWN 0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR 2
#define EXT2_FT_CHRDEV 3
#define EXT2_FT_BLKDEV 4
#define EXT2_FT_FIFO 5
#define EXT2_FT_SOCK 6
#define EXT2_FT_SYMLINK 7
    uint8_t name[0];
} __attribute__((packed));

/* Values for old filesystems (that have the good old revision) */
#define EXT2_GOOD_OLD_INODE_SIZE 128

#endif /* __EXT2_H__ */
