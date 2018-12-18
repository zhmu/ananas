#include "relocate.h"
#include <ananas/elf.h>
#include <loader/module.h>
#include "io.h"
#include "lib.h"

#if DEBUG
#define ELF_DEBUG(fmt, ...) printf("ELF: " fmt, ##__VA_ARGS__)
#else
#define ELF_DEBUG(...)
#endif

/*
 * We expect an i386/amd64 ELF kernel to be concatinated to our Multiboot
 * stub.
 */
extern void* __end;

static int verify_kernel(uint32_t* ph_offset, size_t* ph_entsize, int* ph_num, uint64_t* entry)
{
    /* Check ELF magic first; this doesn't depend on 32/64 bit fields */
    const Elf64_Ehdr* ehdr = (Elf64_Ehdr*)&__end;
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3)
        panic("bad ELF magic");
    if (ehdr->e_ident[EI_VERSION] != EV_CURRENT)
        panic("bad ELF version");
    if (ehdr->e_type != ET_EXEC)
        panic("bad type (not executable)");

    /* Now dive into the details */
    if (ehdr->e_machine == EM_X86_64 && ehdr->e_ident[EI_CLASS] == ELFCLASS64) {
        /* amd64 */
        Elf64_Ehdr* ehdr64 = (Elf64_Ehdr*)ehdr;
        if (ehdr64->e_phnum == 0)
            panic("not a static binary");
        if (ehdr64->e_phentsize < sizeof(Elf64_Phdr))
            panic("pheader too small");
        if (ehdr64->e_shentsize < sizeof(Elf64_Shdr))
            panic("sheader too small");

        *ph_offset = ehdr64->e_phoff;
        *ph_entsize = ehdr64->e_phentsize;
        *ph_num = ehdr64->e_phnum;
        *entry = ehdr64->e_entry;
        return 64;
    } else if (ehdr->e_machine == EM_386 && ehdr->e_ident[EI_CLASS] == ELFCLASS32) {
        /* i386 */
        Elf32_Ehdr* ehdr32 = (Elf32_Ehdr*)ehdr;
        if (ehdr32->e_phnum == 0)
            panic("not a static binary");
        if (ehdr32->e_phentsize < sizeof(Elf32_Phdr))
            panic("pheader too small");
        if (ehdr32->e_shentsize < sizeof(Elf32_Shdr))
            panic("sheader too small");

        *ph_offset = ehdr32->e_phoff;
        *ph_entsize = ehdr32->e_phentsize;
        *ph_num = ehdr32->e_phnum;
        *entry = ehdr32->e_entry;
        return 32;
    }

    panic("bad machine/class combination");
}

/* Reads a program header at offset and convert it to a 64-bit phdr if needed */
static const Elf64_Phdr* fetch_phdr(int bits, uint32_t offset)
{
    switch (bits) {
        case 32: {
            const Elf32_Phdr* phdr_in = (const Elf32_Phdr*)((char*)&__end + offset);
            static Elf64_Phdr phdr_out;
            phdr_out.p_type = phdr_in->p_type;
            phdr_out.p_flags = phdr_in->p_flags;
            phdr_out.p_offset = phdr_in->p_offset;
            phdr_out.p_vaddr = phdr_in->p_vaddr;
            phdr_out.p_paddr = phdr_in->p_paddr;
            phdr_out.p_filesz = phdr_in->p_filesz;
            phdr_out.p_memsz = phdr_in->p_memsz;
            phdr_out.p_align = phdr_in->p_align;
            return &phdr_out;
        }
        case 64:
            return (const Elf64_Phdr*)((char*)&__end + offset);
    }
    return NULL; /* shouldn't get here */
}

void relocate_kernel(struct RELOCATE_INFO* ri)
{
    /*
     * Determine the type of kernel we have, plus the program header
     * base/size.
     */
    uint32_t phdr_base;
    size_t phdr_size;
    int phdr_num;
    ri->ri_bits = verify_kernel(&phdr_base, &phdr_size, &phdr_num, &ri->ri_entry);
    if (ri->ri_bits != 32 && ri->ri_bits != 64)
        panic("unrecognized bits %d", ri->ri_bits);

    ri->ri_vaddr_start = ~0;
    ri->ri_vaddr_end = 0;
    ri->ri_paddr_start = ~0;
    ri->ri_paddr_end = 0;
    for (unsigned int n = 0; n < phdr_num; n++) {
        const Elf64_Phdr* phdr = fetch_phdr(ri->ri_bits, phdr_base + n * phdr_size);
        if (phdr->p_type != PT_LOAD)
            continue;

        void* dest = (void*)(uint32_t)phdr->p_paddr;
        if (phdr->p_memsz > 0) {
            /*
             * Prevent overflow; besides, we can't help less in memory than what to
             * copy so this would be bogus ELF anyway.
             */
            if (phdr->p_memsz < phdr->p_filesz)
                panic("ph #%d: memsz (0x%x) < filesz (0x%x)", n, phdr->p_memsz, phdr->p_filesz);
            void* clear_ptr = (void*)((addr_t)dest + (addr_t)phdr->p_filesz);
            uint32_t clear_len = phdr->p_memsz - phdr->p_filesz;
            ELF_DEBUG("ph #%d: clearing %d bytes @ %p\n", n, clear_len, clear_ptr);
            memset(clear_ptr, 0, clear_len);
        }
        if (phdr->p_filesz > 0) {
            ELF_DEBUG(
                "ph #%d: copying %d bytes from 0x%x -> %p\n", n, (uint32_t)phdr->p_filesz,
                (uint32_t)phdr->p_offset, dest);
            memcpy(dest, (char*)&__end + phdr->p_offset, phdr->p_filesz);
        }

#define MIN(a, b)  \
    if ((b) < (a)) \
    (a) = (b)
#define MAX(a, b)  \
    if ((b) > (a)) \
    (a) = (b)

        MIN(ri->ri_vaddr_start, phdr->p_vaddr);
        MAX(ri->ri_vaddr_end, phdr->p_vaddr + phdr->p_memsz);
        MIN(ri->ri_paddr_start, phdr->p_paddr);
        MAX(ri->ri_paddr_end, phdr->p_paddr + phdr->p_memsz);

#undef MIN
#undef MAX
    }

#define PRINT64(v) (uint32_t)((v) >> 32), (uint32_t)((v)&0xffffffff)

    ELF_DEBUG(
        "loading done, bits=%d, entry=%x:%x, phys=%x:%x .. %x:%x, virt=%x:%x .. %x:%x\n",
        ri->ri_bits, PRINT64(ri->ri_entry), PRINT64(ri->ri_paddr_start), PRINT64(ri->ri_paddr_end),
        PRINT64(ri->ri_vaddr_start), PRINT64(ri->ri_vaddr_end));
}
