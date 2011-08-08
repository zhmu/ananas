#include <ananas/types.h>
#include <loader/vfs.h>
#include <loader/lib.h>
#include <loader/elf.h>
#include <loader/module.h>
#include <loader/platform.h>
#include <machine/param.h> /* for PAGE_SIZE */
#include <elf.h>

#if defined(ELF32) || defined(ELF64)

#undef DEBUG_ELF

#define ELF_ABORT(x) \
	do { \
		puts(x); \
		return 0; \
	} while (0)

#ifdef ELF32
static int
elf32_load_kernel(void* header, struct LOADER_MODULE* mod)
{
	Elf32_Ehdr* ehdr = header;

	/* We only support static binaries for now, so reject anything without a program table */
	if (ehdr->e_phnum == 0)
		ELF_ABORT("not a static binary");
	if (ehdr->e_phentsize < sizeof(Elf32_Phdr))
		ELF_ABORT("pheader too small");
	if (ehdr->e_shentsize < sizeof(Elf32_Shdr))
		ELF_ABORT("sheader too small");

	/*
	 * Read all program table entries in a single go; this prevents us from having to
	 * seek back and forth (which TFTP does not support). The way we do this is a
	 * kludge; we ask for no memory as we know it's increasing (this way we don't have to
	 * free anything)
	 */
	void* phent = platform_get_memory(0);
	if (vfs_pread(phent, ehdr->e_phnum * ehdr->e_phentsize, ehdr->e_phoff) != ehdr->e_phnum * ehdr->e_phentsize)
		ELF_ABORT("pheader read error");
	/* XXX we should attempt to sort the phent's to ensure we can stream the input file */

	for (unsigned int i = 0; i < ehdr->e_phnum; i++) {
		Elf32_Phdr* phdr = (Elf32_Phdr*)(phent + i * ehdr->e_phentsize);
		if (phdr->p_type != PT_LOAD)
			continue;

		void* dest = (void*)phdr->p_paddr;
#ifdef DEBUG_ELF
		printf("ELF: reading %u bytes at offset %u to 0x%x\n",
		 phdr->p_filesz, phdr->p_offset, dest);
#endif
		platform_map_memory(dest, phdr->p_memsz);
		memset(dest, 0, phdr->p_memsz);
		if (vfs_pread(dest, phdr->p_filesz, phdr->p_offset) != phdr->p_filesz)
			ELF_ABORT("data read error");
		if (mod->mod_start_addr > phdr->p_vaddr)
			mod->mod_start_addr = phdr->p_vaddr;
		if (mod->mod_end_addr < phdr->p_vaddr + phdr->p_memsz)
			mod->mod_end_addr = phdr->p_vaddr + phdr->p_memsz;
		if (mod->mod_phys_start_addr > phdr->p_paddr)
			mod->mod_phys_start_addr = phdr->p_paddr;
		if (mod->mod_phys_end_addr < phdr->p_paddr + phdr->p_memsz)
			mod->mod_phys_end_addr = phdr->p_paddr + phdr->p_memsz;
	}

	/*
	 * Walk through the ELF file's section headers; we need this in order to
	 * obtain the string table and symbol table offsets. Together, these make up
	 * the symbol table which is necessary for the kernel to be able to load
	 * modules. We position these tables directly after the kernel.
	 */
	addr_t dest = mod->mod_phys_end_addr;
	void* shent = platform_get_memory(0);
	if (vfs_pread(shent, ehdr->e_shnum * ehdr->e_shentsize, ehdr->e_shoff) != ehdr->e_shnum * ehdr->e_shentsize)
		ELF_ABORT("section header read error");
	for (unsigned int i = 0; i < ehdr->e_shnum; i++) {
		/*
		 * Skip the section string table index; we do not need the section
		 * table headers. Note that index zero (SHN_UNDEF) is reserved and
		 * will always be of type SHT_NULL, so we needn't any special
		 * handling.
		 */
		if (ehdr->e_shstrndx == i)
			continue;
		Elf32_Shdr* shdr = (Elf32_Shdr*)(shent + i * ehdr->e_shentsize);
		switch(shdr->sh_type) {
			case SHT_SYMTAB:
				mod->mod_symtab_addr = dest;
				mod->mod_symtab_size = shdr->sh_size;
#ifdef DEBUG_ELF
				printf("ELF: symtab @ %p, %u bytes\n", dest, shdr->sh_size);
#endif
				break;
			case SHT_STRTAB:
				mod->mod_strtab_addr = dest;
				mod->mod_strtab_size = shdr->sh_size;
#ifdef DEBUG_ELF
				printf("ELF: strtab @ %p, %u bytes\n", dest, shdr->sh_size);
#endif
				break;
			default:
				continue;
		}

		if (vfs_pread((void*)dest, shdr->sh_size, shdr->sh_offset) != shdr->sh_size)
			ELF_ABORT("unable to read section");
		dest += shdr->sh_size;
	}

	mod->mod_phys_end_addr = dest; /* adjust for tables */
	mod->mod_entry = (uint64_t)ehdr->e_entry;
	mod->mod_type = MOD_KERNEL;
	mod->mod_bits = 32;
	return 1;
}
#endif /* ELF32 */

static int
elf_generic_load_module(void* header_data, struct LOADER_MODULE* mod)
{
	/*
	 * Loading a module is easy: we have already performed the ELF checks, so
	 * we'll just read the module to a convient location and let the kernel deal
	 * with the hard stuff like relocations and symbols. The reason is that the
	 * kernel needs to do these things anyway because it should be capable of
	 * loading modules on the fly, and thus we needn't bloat the loader with it.
	 *
	 * Only downside is that it's not possible to know whether loading a module
	 * will succeed in the loader (but then again, a module can always refuse to
	 * initialize so this isn't that much of an issue)
	 */
	addr_t dest = mod_kernel.mod_phys_end_addr;
	uint32_t offs = sizeof(Elf32_Ehdr); /* current location in the file */
	memcpy((void*)dest, header_data, offs);
	dest += offs;
	for (;;) {
		size_t len = vfs_pread((void*)dest, 1024, offs);
		if (len == 0)
			break;
		dest += len; offs += len;
	}

	mod->mod_type = MOD_MODULE;
	mod->mod_bits = mod_kernel.mod_bits; /* XXX */
	mod->mod_phys_start_addr = mod_kernel.mod_phys_end_addr;
	mod->mod_phys_end_addr = dest;
	mod_kernel.mod_phys_end_addr = dest;
	return 1;
}

int
elf_load(enum MODULE_TYPE type, struct LOADER_MODULE* mod)
{
	/* Reset loaded status */
	memset(mod, 0, sizeof(struct LOADER_MODULE));
	mod->mod_start_addr = (uint64_t)-1;
	mod->mod_phys_start_addr = (uint64_t)-1;

	/*
	 * Grab the ELF header. We just read a meager 32 bit header to ensure we
	 * will never read too much (some VFS drivers cannot seek, so we must stream
	 * if we can) - if this happens to be a 64-bit kernel, we'll just read the
	 * remaining part.
	 */
	char header_data[sizeof(Elf64_Ehdr)];
	if (vfs_pread(header_data, sizeof(Elf32_Ehdr), 0) != sizeof(Elf32_Ehdr))
		ELF_ABORT("header read error");

	/* Perform basic ELF checks; identical for ELF32/64 cases */
	Elf32_Ehdr* ehdr = (void*)header_data;
	if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3)
		ELF_ABORT("bad header magic");
	if (ehdr->e_ident[EI_VERSION] != EV_CURRENT)
		ELF_ABORT("bad version");
	if (type == MOD_KERNEL && ehdr->e_type != ET_EXEC)
		ELF_ABORT("bad type (not a kernel)");
	if (type == MOD_MODULE && ehdr->e_type != ET_REL)
		ELF_ABORT("bad type (not a module)");
#ifdef __i386__
	if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB)
		ELF_ABORT("not i386 LSB");
	if (ehdr->e_machine != EM_386)
		ELF_ABORT("not i386");
#elif defined(__PowerPC__)
	if (ehdr->e_ident[EI_DATA] != ELFDATA2MSB)
		ELF_ABORT("not ppc MSB");
#endif

	switch(ehdr->e_ident[EI_CLASS]) {
		case ELFCLASS32:
#ifdef ELF32
			if (!platform_is_numbits_capable(32))
				ELF_ABORT("platform is not 32-bit capable");
			if (type == MOD_KERNEL)
				return elf32_load_kernel(header_data, mod);
			if (type == MOD_MODULE)
				return elf_generic_load_module(header_data, mod);
#else
			ELF_ABORT("32 bit ELF files not supported");
#endif
			break;
		case ELFCLASS64:
			ELF_ABORT("64 bit ELF files not supported");
			break;
		default:
			ELF_ABORT("bad class");
	}

	/* What's this? */
	return 0;
}
#endif /* defined(ELF32) || defined(ELF64) */

/* vim:set ts=2 sw=2: */
