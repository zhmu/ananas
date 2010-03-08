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

	unsigned int i;
	for (i = 0; i < ehdr->e_phnum; i++) {
		Elf64_Phdr* phdr = (Elf64_Phdr*)(data + ehdr->e_phoff + i * ehdr->e_phentsize);
		if (phdr->p_type != PT_LOAD)
			continue;

		/*
		 * The program need not begin at a page-size, so we may need to adjust.
		 */
		int delta = phdr->p_vaddr % PAGE_SIZE;
		struct THREAD_MAPPING* tm = thread_mapto(thread, phdr->p_vaddr - delta, NULL, phdr->p_memsz + delta, THREAD_MAP_ALLOC);
		memcpy(tm->ptr, (void*)(data + phdr->p_offset + delta), phdr->p_filesz);
	}

	/* XXX amd64 */
	struct MD_THREAD* md = (struct MD_THREAD*)thread->md;
	md->ctx.sf.sf_rip = ehdr->e_entry;
	return 1;
}

/* vim:set ts=2 sw=2: */
