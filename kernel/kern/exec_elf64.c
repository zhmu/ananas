#include "types.h"
#include "elf.h"
#include "lib.h"
#include "thread.h"
#include "machine/thread.h"
#include "mm.h"

int
elf64_load(thread_t thread, const char* data, size_t datalen)
{
	Elf64_Ehdr* ehdr = (Elf64_Ehdr*)data;

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

	/* XXX amd64 specific */
	if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB)
		return 0;
	if (ehdr->e_machine != EM_X86_64)
		return 0;

	/* We only support static binaries for now, so reject anything without a program table */
	if (ehdr->e_phnum == 0)
		return 0;
	if (ehdr->e_phentsize < sizeof(Elf64_Phdr))
		return 0;

	/* XXX amd64 */
	struct MD_THREAD* md = (struct MD_THREAD*)thread->md;

	unsigned int i;
	for (i = 0; i < ehdr->e_phnum; i++) {
		Elf64_Phdr* phdr = (Elf64_Phdr*)(data + ehdr->e_phoff + i * ehdr->e_phentsize);
		if (phdr->p_type != PT_LOAD)
			continue;

		/*
	 	 * Allocate memory and copy the image in place.
		 */
		uint8_t* buf = (uint8_t*)kmalloc(phdr->p_memsz);
		memcpy(buf, (void*)(data + phdr->p_offset), phdr->p_filesz);

		/* XXX amd64 */
		int num_pages = (phdr->p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;
		vm_mapto_pagedir(md->pml4, phdr->p_vaddr, buf, num_pages, 1);
	}

	/* XXX amd64 */
	md->ctx.sf.sf_rip = ehdr->e_entry;
	return 1;
}

/* vim:set ts=2 sw=2: */
