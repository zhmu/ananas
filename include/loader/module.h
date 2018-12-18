/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __LOADER_MODULE_H__
#define __LOADER_MODULE_H__

struct BOOTINFO;

enum MODULE_TYPE { MOD_KERNEL, MOD_MODULE, MOD_RAMDISK };

struct LOADER_MODULE {
    int mod_bits;                   /* number of bits, 0 if nothing loaded */
    enum MODULE_TYPE mod_type;      /* module type */
    uint64_t mod_entry;             /* entry point (virtual) */
    uint64_t mod_start_addr;        /* first address (virtual) */
    uint64_t mod_end_addr;          /* last address (virtual) */
    uint64_t mod_phys_start_addr;   /* first address (physical) */
    uint64_t mod_phys_end_addr;     /* last address (physical) */
    uint64_t mod_symtab_addr;       /* symbol table address (physical) */
    uint64_t mod_symtab_size;       /* symbol table length */
    uint64_t mod_strtab_addr;       /* string table address (physical) */
    uint64_t mod_strtab_size;       /* string table length */
    struct LOADER_MODULE* mod_next; /* pointer to next module */
};

#ifndef KERNEL
int module_load(enum MODULE_TYPE type);
void module_make_bootinfo(struct BOOTINFO** bootinfo);

extern struct LOADER_MODULE mod_kernel;
#endif

#endif /* __LOADER_MODULE_H__ */
