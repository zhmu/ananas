#include <ananas/types.h>
#include <loader/vfs.h>
#include <loader/lib.h>
#include <loader/elf.h>
#include <loader/platform.h>
#include <elf.h>

#if defined(ELF32) || defined(ELF64)

#define DEBUG_ELF

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

#ifdef ELF32
static int
elf32_load(void* header, struct LOADER_ELF_INFO* elf_info)
{
	Elf32_Ehdr* ehdr = header;

#ifdef __i386__
	if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB)
		ELF_ABORT("not i386 LSB");
	if (ehdr->e_machine != EM_386)
		ELF_ABORT("not i386");
#elif defined(__PowerPC__)
	if (ehdr->e_ident[EI_DATA] != ELFDATA2MSB)
		ELF_ABORT("not ppc MSB");
#endif

	/* We only support static binaries for now, so reject anything without a program table */
	if (ehdr->e_phnum == 0)
		ELF_ABORT("not a static binary");
	if (ehdr->e_phentsize < sizeof(Elf32_Phdr))
		ELF_ABORT("pheader too small");

	/*
	 * Read all program table entries in a single go; this prevents us from having to
	 * seek back and forth (which TFTP does not support). The way we do this is a
	 * kludge; we ask for no memory as we know it's increasing (this way we don't have to
	 * free anything)
	 */
	void* phent = platform_get_memory(0);
	if (vfs_pread(phent, ehdr->e_phnum * sizeof(Elf32_Phdr), ehdr->e_phoff) != ehdr->e_phnum * sizeof(Elf32_Phdr))
		ELF_ABORT("pheader read error");
	/* XXX we should attempt to sort the phent's to ensure we can stream the input file */

	for (unsigned int i = 0; i < ehdr->e_phnum; i++) {
		Elf32_Phdr* phdr = (Elf32_Phdr*)(phent + i * ehdr->e_phentsize);
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
		if (elf_info->elf_start_addr > phdr->p_vaddr)
			elf_info->elf_start_addr = phdr->p_vaddr;
		if (elf_info->elf_end_addr < phdr->p_vaddr + phdr->p_memsz)
			elf_info->elf_end_addr = phdr->p_vaddr + phdr->p_memsz;
		if (elf_info->elf_phys_start_addr > phdr->p_paddr)
			elf_info->elf_phys_start_addr = phdr->p_paddr;
		if (elf_info->elf_phys_end_addr < phdr->p_paddr + phdr->p_memsz)
			elf_info->elf_phys_end_addr = phdr->p_paddr + phdr->p_memsz;
	}

	elf_info->elf_entry = (uint64_t)ehdr->e_entry;
	elf_info->elf_bits = 32;
	return 1;
}
#endif /* ELF32 */

#ifdef ELF64
static int
elf64_load(void* header , struct LOADER_ELF_INFO* elf_info)
{
	Elf64_Ehdr* ehdr = header;

	/* Perform basic ELF checks */
	if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB)
		return 0;
	if (ehdr->e_machine != EM_X86_64)
		return 0;

#ifdef __i386__
	if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB)
		ELF_ABORT("not i386 LSB");
	if (ehdr->e_machine != EM_X86_64)
		ELF_ABORT("not x86_64");
#elif defined(__PowerPC__)
	ELF_ABORT("powerpc64 is not supported yet");
#endif
	/* We only support static binaries for now, so reject anything without a program table */
	if (ehdr->e_phnum == 0)
		ELF_ABORT("not a static binary");
	if (ehdr->e_phentsize < sizeof(Elf64_Phdr))
		ELF_ABORT("pheader too small");

	/*
	 * Read all program table entries in a single go; this prevents us from having to
	 * seek back and forth (which TFTP does not support). The way we do this is a
	 * kludge; we ask for no memory as we know it's increasing (this way we don't have to
	 * free anything)
	 */
	void* phent = platform_get_memory(0);
	if (vfs_pread(phent, ehdr->e_phnum * sizeof(Elf64_Phdr), ehdr->e_phoff) != ehdr->e_phnum * sizeof(Elf64_Phdr))
		ELF_ABORT("pheader read error");
	/* XXX we should attempt to sort the phent's to ensure we can stream the input file */

	for (unsigned int i = 0; i < ehdr->e_phnum; i++) {
		Elf64_Phdr* phdr = (Elf64_Phdr*)(phent + i * ehdr->e_phentsize);
		if (phdr->p_type != PT_LOAD)
			continue;

		if (phdr->p_paddr > 0x100000000)
			ELF_ABORT("section not present in first 4gb");

		uint32_t paddr = phdr->p_paddr & 0xfffffff; /* XXX kludge */
		memset((void*)paddr, 0, phdr->p_memsz);
#ifdef DEBUG_ELF
		printf("ELF: reading %u bytes at offset %u to 0x%x\n",
		 (int)phdr->p_filesz, (int)phdr->p_offset, paddr);
#endif
		if (vfs_pread(paddr, phdr->p_filesz, phdr->p_offset) != phdr->p_filesz)
			ELF_ABORT("data read error");
		if (elf_info->elf_start_addr > phdr->p_vaddr)
			elf_info->elf_start_addr = phdr->p_vaddr;
		if (elf_info->elf_end_addr < phdr->p_vaddr + phdr->p_memsz)
			elf_info->elf_end_addr = phdr->p_vaddr + phdr->p_memsz;
		if (elf_info->elf_phys_start_addr > paddr)
			elf_info->elf_phys_start_addr = paddr;
		if (elf_info->elf_phys_end_addr < paddr + phdr->p_memsz)
			elf_info->elf_phys_end_addr = paddr + phdr->p_memsz;
	}

	elf_info->elf_entry = ehdr->e_entry;
	elf_info->elf_bits = 64;
	return 1;

}
#endif /* ELF64 */

int
elf_load(struct LOADER_ELF_INFO* elf_info)
{
	/* Reset loaded status */
	elf_info->elf_bits = 0;
	elf_info->elf_start_addr = elf_info->elf_phys_start_addr = 0xffffffffffffffff;
	elf_info->elf_end_addr = elf_info->elf_phys_end_addr = 0;

	/*
	 * Grab the kernel header. In a 32-bit case, we may read a bit too much, but
	 * that is alright as we won't use it.
	 */
	char header_data[sizeof(Elf64_Ehdr)];
	if (vfs_pread(header_data, sizeof(header_data), 0) != sizeof(header_data))
		ELF_ABORT("header read error");

	/* Perform basic ELF checks; identical for ELF32/64 cases */
	Elf32_Ehdr* ehdr = (void*)header_data;
	if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3)
		ELF_ABORT("bad header magic");
	if (ehdr->e_ident[EI_VERSION] != EV_CURRENT)
		ELF_ABORT("bad version");
	if (ehdr->e_type != ET_EXEC)
		ELF_ABORT("bad type");

	switch(ehdr->e_ident[EI_CLASS]) {
		case ELFCLASS32:
#ifdef ELF32
			return elf32_load(header_data, elf_info);
#else
			ELF_ABORT("32 bit ELF files not supported");
#endif
			break;
		case ELFCLASS64:
#ifdef ELF64
			return elf64_load(header_data, elf_info);
#else
			ELF_ABORT("64 bit ELF files not supported");
#endif
			break;
		default:
			ELF_ABORT("bad class");
	}

	/* NOTREACHED */
}

#endif /* defined(ELF32) || defined(ELF64) */

/* vim:set ts=2 sw=2: */
