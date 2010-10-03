#ifndef __LOADER_ELF_H__
#define __LOADER_ELF_H__

struct LOADER_ELF_INFO {
	int		elf_bits;	/* number of bit kernel, 0 if nothing loaded */
	uint64_t	elf_entry;	/* entry point */
	uint64_t	elf_start_addr;	/* first address (virtual) */
	uint64_t	elf_end_addr;	/* last address (virtual) */
	uint64_t	elf_phys_start_addr;	/* first address (physical) */
	uint64_t	elf_phys_end_addr;	/* last address (physical) */
};

int elf_load(struct LOADER_ELF_INFO* elf_info);

#endif /* __LOADER_ELF_H__ */
