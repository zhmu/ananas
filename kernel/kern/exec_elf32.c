#include "types.h"
#include "elf.h"
#include "lib.h"
#include "thread.h"
#include "machine/thread.h"
#include "mm.h"

int
elf32_load(thread_t thread, const char* data, size_t datalen)
{
	Elf32_Ehdr* ehdr = (Elf32_Ehdr*)data;

	/* Perform basic ELF checks */
	if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3)
		return -1;
	if (ehdr->e_ident[EI_CLASS] != ELFCLASS32)
		return -1;
	if (ehdr->e_ident[EI_VERSION] != EV_CURRENT)
		return -1;
	if (ehdr->e_type != ET_EXEC)
		return -1;

	/* XXX i386 specific */
	if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB)
		return -1;
	if (ehdr->e_machine != EM_386)
		return -1;

	/* We only support static binaries for now, so reject anything without a program table */
	if (ehdr->e_phnum == 0)
		return -1;
	/*kprintf("phentsize, struct=%u,%u\n", ehdr->e_phentsize, sizeof(Elf32_Phdr));*/
	if (ehdr->e_phentsize < sizeof(Elf32_Phdr))
		return -1;

	unsigned int i;
	for (i = 0; i < ehdr->e_phnum; i++) {
		Elf32_Phdr* phdr = (Elf32_Phdr*)(data + ehdr->e_phoff + i * ehdr->e_phentsize);
		if (phdr->p_type != PT_LOAD)
			continue;

		/*
		 * The program need not begin at a page-size, so we may need to adjust.
		 */
		int delta = phdr->p_vaddr % PAGE_SIZE;
		struct THREAD_MAPPING* tm = thread_mapto(thread, phdr->p_vaddr - delta, NULL, phdr->p_memsz + delta, THREAD_MAP_ALLOC);
		memcpy(tm->ptr, (void*)(data + phdr->p_offset + delta), phdr->p_filesz);

/*
	 	kprintf("phdr %u, type=%x,offs=%x,vaddr=%x,paddr=%x,filesz=%u,memsz=%u,flags=%x,align=%x\n",
		 i, phdr->p_type, phdr->p_offset, phdr->p_vaddr, phdr->p_paddr, phdr->p_filesz,
		 phdr->p_memsz, phdr->p_flags, phdr->p_align);
*/
	}

	/* XXX i386 */
	struct MD_THREAD* md = (struct MD_THREAD*)thread->md;
	md->ctx.eip = ehdr->e_entry;

	return 0;
}

/* vim:set ts=2 sw=2: */
