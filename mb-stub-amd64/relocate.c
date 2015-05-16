#include "relocate.h"
#include <elf.h>
#include <loader/module.h>
#include "io.h"
#include "lib.h"

#if DEBUG
# define ELF_DEBUG(fmt,...) printf("ELF: "fmt, ##__VA_ARGS__)
#else
# define ELF_DEBUG(...)
#endif

void
relocate_kernel(struct RELOCATE_INFO* ri)
{
	extern void* __end;

	/*
	 * We expect an amd64 ELF kernel to be concatinated to our trampoline code;
	 * first of all, see if the ELF header makes sense.
	 */
	const Elf64_Ehdr* ehdr = (Elf64_Ehdr*)&__end;
	if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1 || 
	    ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3)
		panic("bad ELF magic");
	if (ehdr->e_ident[EI_VERSION] != EV_CURRENT)
		panic("bad ELF version");
	if (ehdr->e_type != ET_EXEC)
		panic("bad type (not executable)");
	if (ehdr->e_machine != EM_X86_64)
		panic("bad architecture");
	if (ehdr->e_ident[EI_CLASS] != ELFCLASS64)
		panic("bad class");
	if (ehdr->e_phnum == 0)
		panic("not a static binary");
	if (ehdr->e_phentsize < sizeof(Elf64_Phdr))
		panic("pheader too small");
	if (ehdr->e_shentsize < sizeof(Elf64_Shdr))
		panic("sheader too small");

	ri->ri_vaddr_start = ~0;
	ri->ri_vaddr_end = 0;
	ri->ri_paddr_start = ~0;
	ri->ri_paddr_end = 0;
	for (unsigned int n = 0; n < ehdr->e_phnum; n++) {
		const Elf64_Phdr* phdr = (const Elf64_Phdr*)((char*)&__end + ehdr->e_phoff + n * ehdr->e_phentsize);
		if (phdr->p_type != PT_LOAD)
			continue;

		void* dest = (void*)(uint32_t)phdr->p_paddr;
		if (phdr->p_memsz > 0) {
			/*
			 * Prevent overflow; besides, we can't help less in memory than what to
 			 * copy so this would be bogus ELF anyway.
			 */
			if (phdr->p_memsz < phdr->p_filesz)
				panic("ph #%d: memsz (0x%x) < filesz (0x%x)", n, phdr->p_memsz, phdr->p_filesz);
			void* clear_ptr = (void*)((addr_t)dest + (addr_t)phdr->p_filesz);
			uint32_t clear_len = phdr->p_memsz - phdr->p_filesz;
			ELF_DEBUG("ph #%d: clearing %d bytes @ %p\n", n, clear_len, clear_ptr);
			memset(clear_ptr, 0, clear_len);
		}
		if (phdr->p_filesz > 0) {
			ELF_DEBUG("ph #%d: copying %d bytes from 0x%x -> %p\n",
			 n, (uint32_t)phdr->p_filesz, (uint32_t)phdr->p_offset, dest);
			memcpy(dest, (char*)&__end + phdr->p_offset, phdr->p_filesz);
		}

#define MIN(a, b) if ((b) < (a)) (a) = (b)
#define MAX(a, b) if ((b) > (a)) (a) = (b)

	MIN(ri->ri_vaddr_start, phdr->p_vaddr);
	MAX(ri->ri_vaddr_end, phdr->p_vaddr + phdr->p_memsz);
	MIN(ri->ri_paddr_start, phdr->p_paddr);
	MAX(ri->ri_paddr_end, phdr->p_paddr + phdr->p_memsz);

#undef MIN
#undef MAX
	}

#define PRINT64(v) \
	 (uint32_t)((v) >> 32), \
	 (uint32_t)((v) & 0xffffffff)

	ELF_DEBUG("loading done, entry=%x:%x, phys=%x:%x .. %x:%x, virt=%x:%x .. %x:%x\n",
	 PRINT64(ehdr->e_entry),
	 PRINT64(ri->ri_paddr_start),
	 PRINT64(ri->ri_paddr_end),
	 PRINT64(ri->ri_vaddr_start),
	 PRINT64(ri->ri_vaddr_end));

	ri->ri_entry = ehdr->e_entry;
}
