/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANAS_VM_OPTIONS_H
#define ANANAS_VM_OPTIONS_H

typedef enum {
    /* Creates a new mapping */
    OP_MAP,

    /* Removes a mapping - only va_addr/va_len are used */
    OP_UNMAP,

    /* Change permissions of a mapping - only va_addr/va_len/va_flags are used */
    OP_CHANGE_ACCESS
} VMOP_OPERATION;

/* Permissions, can be combined */
#define VMOP_FLAG_READ 0x0001
#define VMOP_FLAG_WRITE 0x0002
#define VMOP_FLAG_EXECUTE 0x0004

/* One must be set - SHARED means changes are global */
#define VMOP_FLAG_SHARED 0x0008
#define VMOP_FLAG_PRIVATE 0x0010

/* If set, vo_handle / vo_offset will back the mapping */
#define VMOP_FLAG_FD 0x0020

/* If set, force mapping to be placed here */
#define VMOP_FLAG_FIXED 0x0040

struct VMOP_OPTIONS {
    size_t vo_size; /* must be sizeof(VMOP_OPTIONS) */
    VMOP_OPERATION vo_op;

    /* Address and length of the mapping */
    void* vo_addr; /* Updated on OP_MAP */
    size_t vo_len;

    /* Flags to use */
    int vo_flags;

    /* Backing handle/offset - only if VMOP_FLAG_FD is used */
    fdindex_t vo_fd;
    off_t vo_offset;
};

#endif /* ANANAS_VM_OPTIONS_H */
