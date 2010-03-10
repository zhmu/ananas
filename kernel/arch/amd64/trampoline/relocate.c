#include <types.h>
#include "elf.h"
#include "trampoline.h"

extern unsigned char* kernel;

uint64_t kernel_rip, kernel_end;

int
relocate_elf64(void* data, uint64_t* rip, uint64_t* end)
{
	Elf64_Ehdr* ehdr = (Elf64_Ehdr*)data;
	*end = 0;

	/* Perform basic ELF checks; must be 64 bit LSB statically-linked executable */
	if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3)
		return 0;
	if (ehdr->e_ident[EI_CLASS] != ELFCLASS64)
		return 0;
	if (ehdr->e_ident[EI_VERSION] != EV_CURRENT)
		return 0;
	if (ehdr->e_type != ET_EXEC)
		return 0;
	if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB)
		return 0;
	if (ehdr->e_machine != EM_X86_64)
		return 0;

	/* We only support static binaries for now, so reject anything without a program table */
	if (ehdr->e_phnum == 0)
		return 0;
	if (ehdr->e_phentsize < sizeof(Elf64_Phdr))
		return 0;

	unsigned int i;
	for (i = 0; i < ehdr->e_phnum; i++) {
		Elf64_Phdr* phdr = (Elf64_Phdr*)(data + ehdr->e_phoff + i * ehdr->e_phentsize);
		if (phdr->p_type != PT_LOAD)
			continue;

		/*
	 	 * Copy ELF content in place. XXX should figure out how much memory
	 	 * we have and chop off necessary bits
		 */
		void* dest = (void*)(addr_t)(phdr->p_vaddr & 0x0fffffff);
		memcpy(dest, (void*)(data + phdr->p_offset), phdr->p_filesz);

		/* Store the ending address; this will be passed to the binary */
		if (phdr->p_vaddr + phdr->p_memsz > *end)
			*end = phdr->p_vaddr + phdr->p_memsz;

		/* Create a page mapping from the virtual->physical address */
		size_t size = (phdr->p_filesz + 0x1fffff) / 0x200000;
#define ROUND64_2MB(x) ((x) & 0xffffffffffe00000)
		vm_map(ROUND64_2MB(phdr->p_vaddr), ROUND_2MB((addr_t)dest), size);
	}

	*rip = ehdr->e_entry;
	return 1;
}

int
relocate_kernel()
{
	return relocate_elf64(&kernel, &kernel_rip, &kernel_end);
}

/* vim:set ts=2 sw=2: */
