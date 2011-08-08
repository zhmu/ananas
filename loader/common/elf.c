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

/* Generate the generic ELF32/64 loader functions */
#ifdef ELF32
# define ELF_BITS 32
# include "elf_bits.c"
#endif

#ifdef ELF64
# undef ELF_BITS
# define ELF_BITS 64
# include "elf_bits.c"
#endif

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
	mod->mod_bits = mod_kernel.mod_bits; /* XXX We should check this somehow */
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
	if (
#ifdef ELF32
	ehdr->e_machine != EM_386
#else
	1
#endif
		&&
#ifdef ELF64
	ehdr->e_machine != EM_X86_64
#else
	1
#endif
	)
		ELF_ABORT("unsupported architecture");
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
#ifdef ELF64
			if (!platform_is_numbits_capable(64))
				ELF_ABORT("platform is not 64-bit capable");
			if (type == MOD_KERNEL) {
				/* First of all, read the remaining few bits */
				if (vfs_pread((header_data + sizeof(Elf32_Ehdr)), sizeof(Elf64_Ehdr) - sizeof(Elf32_Ehdr), sizeof(Elf32_Ehdr)) != sizeof(Elf64_Ehdr) - sizeof(Elf32_Ehdr))
					ELF_ABORT("cannot read full ELF64 header");
				return elf64_load_kernel(header_data, mod);
			}
			if (type == MOD_MODULE)
				return elf_generic_load_module(header_data, mod);
#endif
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
