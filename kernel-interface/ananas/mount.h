/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#define MNT_RDONLY 0x0001 /* read only */
#define MNT_NOWAIT 0x0002

/* BSD extensions */
#define MNT_LOCAL  0x1000 /* local volume */

#define MFSNAMELEN 16 /* length of type name including \0 */
#define MNAMELEN 88   /* name buffer length */

typedef int fsid_t;

struct statfs {
    int f_type;    /* filesystem type */
    int f_flags;   /* filesystem flags */
    int f_bsize;   /* optimal block size */
    int f_blocks;  /* total number of blocks */
    int f_bfree;   /* free blocks */
    int f_bavail;  /* available blocks (non-superuser) */
    int f_files;   /* total number of file inodes */
    int f_ffree;   /* total number of free inodes */
    fsid_t f_fsid; /* file system id */
    int f_namelen; /* maximum filename length */
    char f_fstypename[MFSNAMELEN];
    char f_mntfromname[MNAMELEN];
    char f_mntonname[MNAMELEN];
};
