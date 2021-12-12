/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>

#define S_IFMT 0xf000 /* File type */
#define S_IFSOCK 0xc000
#define S_IFLNK 0xa000
#define S_IFREG 0x8000
#define S_IFBLK 0x6000
#define S_IFDIR 0x4000
#define S_IFCHR 0x2000
#define S_IFIFO 0x1000

#define S_ISBLK(m) (((m)&S_IFMT) == S_IFBLK)
#define S_ISCHR(m) (((m)&S_IFMT) == S_IFCHR)
#define S_ISDIR(m) (((m)&S_IFMT) == S_IFDIR)
#define S_ISFIFO(m) (((m)&S_IFMT) == S_IFIFO)
#define S_ISREG(m) (((m)&S_IFMT) == S_IFREG)
#define S_ISLNK(m) (((m)&S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m)&S_IFMT) == S_IFSOCK)

#define S_TYPEISMQ(m) 0  /* XXX unsupported */
#define S_TYPEISSEM(m) 0 /* XXX unsupported */
#define S_TYPEISSHM(m) 0 /* XXX unsupported */

/* File permission bits */
#define S_ISUID 0x0800 /* Set UID */
#define S_ISGID 0x0400 /* Set GID */
#define S_ISVTX 0x0200 /* Delete restriction (directories) */

#define S_IRWXU 0x01c0 /* Read, write, execute/search (owner) */
#define S_IRUSR 0x0100
#define S_IWUSR 0x0080
#define S_IXUSR 0x0040
#define S_IRWXG 0x0038 /* Read, write, execute/search (group) */
#define S_IRGRP 0x0020
#define S_IWGRP 0x0010
#define S_IXGRP 0x0008
#define S_IRXWXO 0x0007 /* Read, write, execute/search (others) */
#define S_IROTH 0x0004
#define S_IWOTH 0x0002
#define S_IXOTH 0x0001

struct stat {
    __dev_t st_dev;
    __ino_t st_ino;
    __mode_t st_mode;
    __nlink_t st_nlink;
    __uid_t st_uid;
    __gid_t st_gid;
    __dev_t st_rdev;
    __off_t st_size;
    __time_t st_atime;
    __time_t st_mtime;
    __time_t st_ctime;
    __blksize_t st_blksize;
    __blkcnt_t st_blocks;
};
