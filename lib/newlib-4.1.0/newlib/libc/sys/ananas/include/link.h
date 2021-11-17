/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANAS_LINK_H
#define ANANAS_LINK_H

#include <sys/types.h>
#include <ananas/amd64/elf.h>
#include <sys/cdefs.h>
#include <stdint.h>

struct dl_phdr_info {
    Elf_Addr dlpi_addr;
    const char* dlpi_name;
    const Elf_Phdr* dlpi_phdr;
    Elf_Half dlpi_phnum;
};

struct link_map {
    Elf_Addr l_addr;      /* base address of library */
    const char* l_name; /* absolute path to library */
    const void* l_ld;   /* pointer to .dynamic in library */
    struct link_map* l_next;
    struct link_map* l_prev;
};

struct r_debug {
    int r_version;
    struct link_map* r_map; /* list of all loaded things */
    void (*r_brk)(
        struct r_debug*, struct link_map*); /* will be called so debuggers can breakpoint on it */

    enum { RT_CONSISTENT, RT_ADD, RT_DELETE } r_status;
};

__BEGIN_DECLS

int dl_iterate_phdr(
    int (*callback)(struct dl_phdr_info* info, size_t size, void* data), void* data);

__END_DECLS

#endif /* ANANAS_LINK_H */
