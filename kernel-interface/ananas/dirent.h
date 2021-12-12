/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANAS_VFS_DIRENT_H
#define ANANAS_VFS_DIRENT_H

#include <ananas/types.h>

#define MAXNAMLEN 255

/*
 * Directory entry, as returned by the kernel.
 */
struct VFS_DIRENT {
    __uint32_t de_flags;      /* Flags */
    __uint8_t de_name_length; /* Length of name */
    __ino_t de_inum;          /* Identifier */
    /*
     * de_name will be stored directly after the fsop.
     */
    char de_name[1];
};
#define DE_LENGTH(x) (sizeof(struct VFS_DIRENT) + (x)->de_name_length)

#endif // ANANAS_VFS_DIRENT_H
