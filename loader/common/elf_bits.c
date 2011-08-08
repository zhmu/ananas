/* Prefixes x with elfNN_; i.e. ELF_MAKE_IDENTIFIER(foo) -> elf32_foo  */
#define ELF_MAKE_IDENTIFIER(x) CONCAT(elf, CONCAT(CONCAT(ELF_BITS, _), x))

/* Prefixes x with ElfNN_ for the ELF types; i.e. ELF_TYPE(Ehdr) -> Elf32_Ehdr */
#define ELF_TYPE(x) CONCAT(Elf, CONCAT(CONCAT(ELF_BITS, _), x))

int
ELF_MAKE_IDENTIFIER(load_kernel)(void* header, struct LOADER_MODULE* mod)
{
	ELF_TYPE(Ehdr)* ehdr = header;

	/* We only support static binaries for now, so reject anything without a program table */
	if (ehdr->e_phnum == 0)
		ELF_ABORT("not a static binary");
	if (ehdr->e_phentsize < sizeof(ELF_TYPE(Phdr)))
		ELF_ABORT("pheader too small");
	if (ehdr->e_shentsize < sizeof(ELF_TYPE(Shdr)))
		ELF_ABORT("sheader too small");

	/*
	 * Read all program table entries in a single go; this prevents us from having to
	 * seek back and forth (which TFTP does not support). The way we do this is a
	 * kludge; we ask for no memory as we know it's increasing (this way we don't have to
	 * free anything)
	 */
	void* phent = platform_get_memory(0);
printf("phnum=%u entsz=%u off=%u\n", ehdr->e_phnum, ehdr->e_phentsize, ehdr->e_phoff);
	if (vfs_pread(phent, ehdr->e_phnum * ehdr->e_phentsize, ehdr->e_phoff) != ehdr->e_phnum * ehdr->e_phentsize)
		ELF_ABORT("pheader read error");
	/* XXX we should attempt to sort the phent's to ensure we can stream the input file */

	for (unsigned int i = 0; i < ehdr->e_phnum; i++) {
		ELF_TYPE(Phdr)* phdr = (ELF_TYPE(Phdr)*)(phent + i * ehdr->e_phentsize);
printf("phdr %u, type %u\n", i, phdr->p_type);
		if (phdr->p_type != PT_LOAD)
			continue;

		void* dest = (void*)(addr_t)phdr->p_paddr;
#ifdef DEBUG_ELF
		printf("ELF: reading %u bytes at offset %u to 0x%x\n",
		 (uint32_t)phdr->p_filesz, (uint32_t)phdr->p_offset, (uint32_t)dest);
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
		ELF_TYPE(Shdr)* shdr = (ELF_TYPE(Shdr)*)(shent + i * ehdr->e_shentsize);
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
	mod->mod_bits = ELF_BITS;
	return 1;
}

/* vim:set ts=2 sw=2: */
