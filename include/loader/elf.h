#ifndef __LOADER_ELF_H__
#define __LOADER_ELF_H__

struct LOADER_ELF_INFO {
	uint64_t	elf_entry;	/* entry point */
	uint64_t	elf_start_addr;	/* first address */
	uint64_t	elf_end_addr;	/* last address */
};

int elf_load(struct LOADER_ELF_INFO* elf_info);

#endif /* __LOADER_ELF_H__ */
