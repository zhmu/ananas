#include <sys/types.h>
#include <loader/vfs.h>
#include <elf.h>
#include <string.h>

static int
elf32_load(uint64_t* entry)
{
	Elf32_Ehdr ehdr;
	if (vfs_pread(&ehdr, sizeof(ehdr), 0) != sizeof(ehdr))
		return 0;

	/* Perform basic ELF checks */
	if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr.e_ident[EI_MAG2] != ELFMAG2 || ehdr.e_ident[EI_MAG3] != ELFMAG3)
		return 0;
	if (ehdr.e_ident[EI_CLASS] != ELFCLASS32)
		return 0;
	if (ehdr.e_ident[EI_VERSION] != EV_CURRENT)
		return 0;
	if (ehdr.e_type != ET_EXEC)
		return 0;

	/* XXX This specifically checks for i386 at the moment */
	if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB)
		return 0;
	if (ehdr.e_machine != EM_386)
		return 0;

	/* We only support static binaries for now, so reject anything without a program table */
	if (ehdr.e_phnum == 0)
		return 0;
	if (ehdr.e_phentsize < sizeof(Elf32_Phdr))
		return 0;

	for (unsigned int i = 0; i < ehdr.e_phnum; i++) {
		Elf32_Phdr phdr;
		if (vfs_pread(&phdr, sizeof(phdr), ehdr.e_phoff + i * ehdr.e_phentsize) != sizeof(phdr))
			return 0;
		if (phdr.p_type != PT_LOAD)
			continue;

		/*
		 * The program need not begin at a page-size, so we may need to adjust.
		 */
		void* dest = (void*)phdr.p_paddr;
		memset(dest, 0, phdr.p_memsz);
		if (vfs_pread(dest, phdr.p_filesz, phdr.p_offset) != phdr.p_filesz)
			return 0;
	}

	*entry = (uint64_t)ehdr.e_entry;
	return 1;
}

int
elf_load(uint64_t* entry)
{
	return elf32_load(entry);
}

/* vim:set ts=2 sw=2: */
