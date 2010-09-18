#include <ananas/types.h>
#include <loader/vfs.h>
#include <loader/lib.h>
#include <loader/platform.h>
#include <elf.h>

#undef DEBUG_ELF

#ifdef DEBUG_ELF
# define ELF_ABORT(x...) \
	do { \
		printf(x); \
		return 0; \
	} while (0)
#else
# define ELF_ABORT(x...) \
	return 0;
#endif

static int
elf32_load(uint64_t* entry)
{
	Elf32_Ehdr ehdr;
	if (vfs_pread(&ehdr, sizeof(ehdr), 0) != sizeof(ehdr))
		ELF_ABORT("header read error");

	/* Perform basic ELF checks */
	if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr.e_ident[EI_MAG2] != ELFMAG2 || ehdr.e_ident[EI_MAG3] != ELFMAG3)
		ELF_ABORT("bad header magic");
	if (ehdr.e_ident[EI_CLASS] != ELFCLASS32)
		ELF_ABORT("bad class");
	if (ehdr.e_ident[EI_VERSION] != EV_CURRENT)
		ELF_ABORT("bad version");
	if (ehdr.e_type != ET_EXEC)
		ELF_ABORT("bad type");

	/* XXX This specifically checks for i386 at the moment */
#ifdef __i386__
	if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB)
		ELF_ABORT("not i386 LSB");
	if (ehdr.e_machine != EM_386)
		ELF_ABORT("not i386");
#elif defined(__PowerPC__)
	if (ehdr.e_ident[EI_DATA] != ELFDATA2MSB)
		ELF_ABORT("not ppc MSB");
#endif

	/* We only support static binaries for now, so reject anything without a program table */
	if (ehdr.e_phnum == 0)
		ELF_ABORT("not a static binary");
	if (ehdr.e_phentsize < sizeof(Elf32_Phdr))
		ELF_ABORT("pheader too small");

	/*
	 * Read all program table entries in a single go; this prevents us from having to
	 * seek back and forth (which TFTP does not support). The way we do this is a
	 * kludge; we ask for no memory as we know it's increasing (this way we don't have to
	 * free anything)
	 */
	void* phent = platform_get_memory(0);
	if (vfs_pread(phent, ehdr.e_phnum * sizeof(Elf32_Phdr), ehdr.e_phoff) != ehdr.e_phnum * sizeof(Elf32_Phdr))
		ELF_ABORT("pheader read error");
	/* XXX we should attempt to sort the phent's to ensure we can stream the input file */

	for (unsigned int i = 0; i < ehdr.e_phnum; i++) {
		Elf32_Phdr* phdr = (Elf32_Phdr*)(phent + i * ehdr.e_phentsize);
		if (phdr->p_type != PT_LOAD)
			continue;

		void* dest = (void*)phdr->p_paddr;
		memset(dest, 0, phdr->p_memsz);
#ifdef DEBUG_ELF
		printf("ELF: reading %u bytes at offset %u to 0x%x\n",
		 phdr->p_filesz, phdr->p_offset, dest);
#endif
		if (vfs_pread(dest, phdr->p_filesz, phdr->p_offset) != phdr->p_filesz)
			ELF_ABORT("data read error");
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
