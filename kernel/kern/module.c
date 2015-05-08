/*
 * Ananas loadable modules are relocatable ELF object files which will be
 * embedded into the kernel. External references are resolved and the
 * module symbols will be made available to other modules.
 *
 * Modules are copied to fresh memory locations upon initialization; this is
 * done so that a crashing module may be re-loaded as necessary. It also
 * makes it possible to just throw away the loaded module's ELF data to save
 * memory, so no functionality is lost.
 */
#include <ananas/types.h>
#include <ananas/bootinfo.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/kmem.h>
#include <ananas/mm.h>
#include <ananas/module.h>
#include <ananas/init.h>
#include <ananas/symbols.h>
#include <ananas/trace.h>
#include <ananas/vm.h>
#include <loader/module.h>
#include <machine/vm.h>
#include <machine/param.h>
#include <elf.h>

/*
 * Debugging is done with plain old printf's because modules are loaded so soon
 * in the boot process that tracing may not be usuable.
 */
#undef DEBUG_LOAD

#ifdef DEBUG_LOAD
# define DPRINTF kprintf
#else
# define DPRINTF(x...)
#endif

/* Maximum number of distinct sections supported */
#define MOD_MAX_SECTIONS 16

TRACE_SETUP;

static mutex_t mtx_modules;
static struct KERNEL_MODULE_QUEUE kernel_modules;

#if ARCH_BITS==32
typedef Elf32_Ehdr Elf_Ehdr;
typedef Elf32_Shdr Elf_Shdr;
typedef Elf32_Sym  Elf_Sym;
typedef Elf32_Rel  Elf_Rel;
#elif ARCH_BITS==64
typedef Elf64_Ehdr Elf_Ehdr;
typedef Elf64_Shdr Elf_Shdr;
typedef Elf64_Sym  Elf_Sym;
typedef Elf64_Rel  Elf_Rel;
#else
# error Unsupported number of architecture bits
#endif

int
module_sym_resolve_name(const char* name, struct SYMBOL* s)
{
	/* XXX locking? */
	if (DQUEUE_EMPTY(&kernel_modules))
		return 0;

	DQUEUE_FOREACH(&kernel_modules, kmod, struct KERNEL_MODULE) {
		Elf_Sym* sym = kmod->kmod_symptr;
		for (unsigned int i = 0; i < kmod->kmod_sym_size; i += sizeof(Elf_Sym), sym++) {
			if (strcmp(kmod->kmod_strptr + sym->st_name, name) != 0)
				continue;
			s->s_addr = sym->st_value;
			s->s_name = kmod->kmod_strptr + sym->st_name;
			s->s_flags = 0; /* TODO */
			return 1;
		}
	}
	return 0;
}

int
module_sym_resolve_addr(addr_t addr, struct SYMBOL* s)
{
	/* XXX locking? */
	if (DQUEUE_EMPTY(&kernel_modules))
		return 0;

	DQUEUE_FOREACH(&kernel_modules, kmod, struct KERNEL_MODULE) {
		Elf_Sym* sym = kmod->kmod_symptr;
		for (unsigned int i = 0; i < kmod->kmod_sym_size; i += sizeof(Elf_Sym), sym++) {
			if (addr < sym->st_value ||
					addr >= sym->st_value + sym->st_size)
				continue;
			s->s_addr = sym->st_value;
			s->s_name = kmod->kmod_strptr + sym->st_name;
			s->s_flags = 0; /* TODO */
			return 1;
		}
	}
	return 0;
}

static errorcode_t
module_load(struct LOADER_MODULE* mod)
{
	addr_t mod_base = PTOKV((addr_t)mod->mod_phys_start_addr);
	Elf_Ehdr* ehdr = (Elf_Ehdr*)mod_base;

	/* Perform basic ELF checks */
	if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr->e_ident[EI_CLASS] != ELFCLASS32)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr->e_ident[EI_VERSION] != EV_CURRENT)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr->e_type != ET_REL)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr->e_ident[EI_DATA] !=
#ifdef LITTLE_ENDIAN
	ELFDATA2LSB
#elif defined(BIG_ENDIAN)
	ELFDATA2MSB
#else
# error What endianness are you?
#endif
	)
		return ANANAS_ERROR(BAD_EXEC);

	/* Modules must not have any program table entries */
	if (ehdr->e_phnum != 0)
		return ANANAS_ERROR(BAD_EXEC);
	/* Modules need to have sections we can read */
	if (ehdr->e_shnum == 0)
		return ANANAS_ERROR(BAD_EXEC);
	if (ehdr->e_shentsize < sizeof(Elf_Shdr))
		return ANANAS_ERROR(BAD_EXEC);

	/*
	 * Walk through the section headers - this walk is just to figure out how
	 * much memory we need and whether everything we need (symbol tables etc) are
	 * there.
	 */
	addr_t elf_sym_tab_offs = 0, elf_sym_tab_size = 0;
	addr_t elf_str_tab_offs = 0, elf_str_tab_size = 0;
	size_t code_size = 0, rodata_size = 0, rwdata_size = 0;
	for (unsigned int i = 0; i < ehdr->e_shnum; i++) {
		Elf_Shdr* shdr = (Elf_Shdr*)(mod_base + ehdr->e_shoff + (i * ehdr->e_shentsize));
		switch(shdr->sh_type) {
			case SHT_PROGBITS:
				/* Skip anything without backing memory */
				if ((shdr->sh_flags & SHF_ALLOC) == 0)
					continue;
				if (i == MOD_MAX_SECTIONS) {
					kprintf("module %p has too many sections - aborting load\n", mod);
					return ANANAS_ERROR(BAD_EXEC);
				}
				if (shdr->sh_flags & SHF_EXECINSTR)
					code_size += shdr->sh_size;
				else if (shdr->sh_flags & SHF_WRITE)
					rwdata_size += shdr->sh_size;
				else
					rodata_size += shdr->sh_size;
				break;
			case SHT_SYMTAB:
				elf_sym_tab_offs = shdr->sh_offset;
				elf_sym_tab_size = shdr->sh_size;
				break;
			case SHT_STRTAB:
				/* If this is the string table for the section names, skip it */
				if (ehdr->e_shstrndx == i)
					continue;
				elf_str_tab_offs = shdr->sh_offset;
				elf_str_tab_size = shdr->sh_size;
				break;
		}
	}
	DPRINTF("symbols at %x(%u), strings at %x(%u)\n",
	 elf_sym_tab_offs, elf_sym_tab_size, elf_str_tab_offs, elf_str_tab_size);
	DPRINTF("code size=%u, rodata size=%u, rwdata size=%u\n",
	 code_size, rodata_size, rwdata_size);

	/* Reject anything without symbols/strings */
	if (elf_sym_tab_offs == 0 || elf_sym_tab_size == 0 ||
	    elf_str_tab_offs == 0 || elf_str_tab_size == 0)
		return ANANAS_ERROR(BAD_EXEC);

	/* Reject anything without code */
	if (code_size == 0)
		return ANANAS_ERROR(BAD_EXEC);

	/*
	 * OK, we know what to allocate; let's do it. Note that we use kmalloc() so
	 * that we have a r/w mapping which is needed to copy the contents around
	 *
	 * Note that this expects kmalloc(NULL) to do the right thing.
	 */
	void* code_ptr   = kmalloc(code_size);
	void* rodata_ptr = kmalloc(rodata_size);
	void* rwdata_ptr = kmalloc(rwdata_size);
	void* sym_ptr    = kmalloc(elf_sym_tab_size);
	void* str_ptr    = kmalloc(elf_str_tab_size);

	/* Store relative section offsets and pointers */
	addr_t section_addr[MOD_MAX_SECTIONS];
	for (unsigned int i = 0; i < MOD_MAX_SECTIONS; i++) {
		section_addr[i] = (addr_t)-1;
	}

	/* Walk through the sections again and copy all data */
	addr_t code_offs = 0, rodata_offs = 0, rwdata_offs = 0;
	for (unsigned int i = 0; i < ehdr->e_shnum; i++) {
		Elf_Shdr* shdr = (Elf_Shdr*)(mod_base + ehdr->e_shoff + (i * ehdr->e_shentsize));
		switch(shdr->sh_type) {
			case SHT_PROGBITS:
				if ((shdr->sh_flags & SHF_ALLOC) == 0)
					continue;
				if (shdr->sh_flags & SHF_EXECINSTR) {
					/* Code */
					memcpy((void*)((addr_t)code_ptr + code_offs), (void*)(mod_base + shdr->sh_offset), shdr->sh_size);
					section_addr[i] = (addr_t)code_ptr + code_offs;
					code_offs += shdr->sh_size;
				} else if (shdr->sh_flags & SHF_WRITE) {
					/* RW Data */
					memcpy((void*)((addr_t)rwdata_ptr + rwdata_offs), (void*)(mod_base + shdr->sh_offset), shdr->sh_size);
					section_addr[i] = (addr_t)rwdata_ptr + rwdata_offs;
					rwdata_offs += shdr->sh_size;
				} else {
					/* RO Data */
					memcpy((void*)((addr_t)rodata_ptr + rodata_offs), (void*)(mod_base + shdr->sh_offset), shdr->sh_size);
					section_addr[i] = (addr_t)rodata_ptr + rodata_offs;
					rodata_offs += shdr->sh_size;
				}
				break;
			case SHT_SYMTAB:
				memcpy(sym_ptr, (void*)(mod_base + shdr->sh_offset), shdr->sh_size);
				break;
			case SHT_STRTAB:
				/* If this is the string table for the section names, skip it */
				if (ehdr->e_shstrndx == i)
					continue;
				memcpy(str_ptr, (void*)(mod_base + shdr->sh_offset), shdr->sh_size);
				break;
		}
	}

	/*
	 * All data is in place; try to resolve all symbols. This is also a good
	 * place to look for the module's init and exit funtions.
	 */
	mod_init_func_t mod_init = NULL;
	mod_exit_func_t mod_exit = NULL;
	for (unsigned int i = 0; i < elf_sym_tab_size; i += sizeof(Elf_Sym)) {
		Elf_Sym* sym = (Elf_Sym*)(sym_ptr + i);

		switch(ELF32_ST_TYPE(sym->st_info)) {
			case STT_NOTYPE: {
				/* Relocate per-section values immediately; we don't need to look them up */
				if (sym->st_shndx != SHN_UNDEF) {
					sym->st_value += section_addr[sym->st_shndx];
					DPRINTF("notype %u: ndx=%u -> %p\n", i, sym->st_shndx, sym->st_value);
					continue;
				}
				/* Skip empty symbols */
				const char* sym_name = (const char*)(str_ptr + sym->st_name);
				if (*sym_name == '\0')
					continue;
				struct SYMBOL s;
				if (!symbol_resolve_name(sym_name, &s) && !module_sym_resolve_name(sym_name, &s)) {
					kprintf("cannot resolve symbol '%s' - aborting module load\n", sym_name);
					goto fail;
				}
				DPRINTF("notype %u: sym '%s' -> %p\n", i, sym_name, s.s_addr);
				sym->st_value = s.s_addr;
				break;
			}
			case STT_OBJECT:
			case STT_FUNC: {
				const char* sym_name = str_ptr + sym->st_name;
				DPRINTF("object/func %u (%s) @ %p -> %p\n", i, sym_name, sym->st_value, sym->st_value + section_addr[sym->st_shndx]);
				sym->st_value += section_addr[sym->st_shndx];
				if (!strcmp(sym_name, MOD_INIT_FUNC_NAME)) {
					mod_init = (mod_init_func_t)sym->st_value;
				} else if (!strcmp(sym_name, MOD_EXIT_FUNC_NAME)) {
					mod_exit = (mod_init_func_t)sym->st_value;
				}
				break;
			}
			case STT_SECTION: {
				DPRINTF("section %u (%u) -> %p\n", i, sym->st_shndx, section_addr[sym->st_shndx]);
				sym->st_value = section_addr[sym->st_shndx];
				break;
			}
		}
	}

	/* Finally, resolve all references */
	for (unsigned int i = 0; i < ehdr->e_shnum; i++) {
		Elf_Shdr* shdr = (Elf_Shdr*)(mod_base + ehdr->e_shoff + (i * ehdr->e_shentsize));
		KASSERT(shdr->sh_type != SHT_RELA, "rela isn't yet supported");
		if (shdr->sh_type != SHT_REL)
			continue;

		if (shdr->sh_info >= MOD_MAX_SECTIONS || section_addr[shdr->sh_info] == (addr_t)-1) {
			kprintf("invalid section %u given as base for relocation %u - aborting module load\n",
			 shdr->sh_info, i);
			goto fail;
		}
		DPRINTF(">> section %u, link %u, info %u, addr %p\n", shdr->sh_name, shdr->sh_link, shdr->sh_info,
			section_addr[shdr->sh_info]);

		/*
		 * Walk through the relocations; these are relative to offset 0 in the
		 * section.
		 */
		Elf_Rel* rel = (Elf_Rel*)(mod_base + shdr->sh_offset);
		for (unsigned int n = 0; n < shdr->sh_size; n += sizeof(Elf_Rel), rel++) {
			Elf_Sym* sym = (Elf_Sym*)(sym_ptr + ELF32_R_SYM(rel->r_info) * sizeof(Elf_Sym));
			DPRINTF("offs=%u, info=%u (sym %u(%s), type %u, val %x)\n",
			 rel->r_offset, rel->r_info,
			 ELF32_R_SYM(rel->r_info), (str_ptr + sym->st_name),
		 	 ELF32_R_TYPE(rel->r_info), sym->st_value);
			addr_t addr = section_addr[shdr->sh_info] + rel->r_offset;
			uint32_t addend = *(uint32_t*)addr;
			switch(ELF32_R_TYPE(rel->r_info)) {
				case R_386_32:
					*(uint32_t*)addr = sym->st_value + addend;
					break;
				case R_386_PC32:
					*(uint32_t*)addr = sym->st_value + addend - addr;
					break;
				default:
					kprintf("unsupported relocation %u - aborted module load\n", ELF32_R_TYPE(rel->r_info));
					goto fail;
			}
		}
	}

	/*
	 * We are done with the relocation bits. Now we should make the rodata
	 * actually readonly, and the code readonly and executable.
	 */
	kmem_map(KVTOP((addr_t)code_ptr), code_size, VM_FLAG_READ | VM_FLAG_EXECUTE | VM_FLAG_KERNEL);
	kmem_map(KVTOP((addr_t)rodata_ptr), rodata_size, VM_FLAG_READ | VM_FLAG_KERNEL);

	/* Set up the module structure */
	struct KERNEL_MODULE* kmod = kmalloc(sizeof(struct KERNEL_MODULE));
	kmod->kmod_init_func = mod_init;
	kmod->kmod_exit_func = mod_exit;
	kmod->kmod_strptr = str_ptr;
	kmod->kmod_symptr = sym_ptr;
	kmod->kmod_sym_size = elf_sym_tab_size;
	kmod->kmod_rwdataptr = rwdata_ptr;
	kmod->kmod_rodataptr = rodata_ptr;
	kmod->kmod_codeptr = code_ptr;

	/* Everything seems to be in order... it's time to call init! */
	errorcode_t err = mod_init(kmod);
	if (err != ANANAS_ERROR_OK) {
		kfree(kmod);
		kprintf("module %p failed to initialize with error %u\n", kmod, err);
		goto fail;
	}

	/* Hook up the module to the list of modules */
	mutex_lock(&mtx_modules);
	DQUEUE_ADD_TAIL(&kernel_modules, kmod);
	mutex_unlock(&mtx_modules);

	return ANANAS_ERROR_OK;

fail:
	kfree(str_ptr);
	kfree(sym_ptr);
	kfree(rwdata_ptr);
	kfree(rodata_ptr);
	kfree(code_ptr);
	return ANANAS_ERROR(BAD_EXEC);
}

errorcode_t
module_unload(struct KERNEL_MODULE* kmod)
{
	/* Remove up the module from the list of modules */
	mutex_lock(&mtx_modules);
	DQUEUE_REMOVE(&kernel_modules, kmod);
	mutex_unlock(&mtx_modules);

	/* Ask it to exit */
	errorcode_t err = kmod->kmod_exit_func(kmod);
	mutex_lock(&mtx_modules);
	if (err != ANANAS_ERROR_OK) {
		kprintf("module %p failed to exit with error %u\n", kmod, err);
		/* Hook the module back in the list - we couldn't unload it */
		DQUEUE_ADD_TAIL(&kernel_modules, kmod);
		mutex_unlock(&mtx_modules);
		return err;
	}

	/* Throw away any pending module init functions */
	init_unregister_module(kmod);
	mutex_unlock(&mtx_modules);

	/* Free all module data */
	kfree(kmod->kmod_strptr);
	kfree(kmod->kmod_symptr);
	kfree(kmod->kmod_rwdataptr);
	kfree(kmod->kmod_rodataptr);
	kfree(kmod->kmod_codeptr);
	kfree(kmod);
	return ANANAS_ERROR_OK;
}

static errorcode_t
module_init()
{
	mutex_init(&mtx_modules, "mtx_modules");
	DQUEUE_INIT(&kernel_modules);
	if (bootinfo == NULL)
		return ANANAS_ERROR(NO_DEVICE);

#ifndef __amd64__
	/* Walk through all modules the loader has for us */
	for (struct LOADER_MODULE* mod = (struct LOADER_MODULE*)PTOKV((addr_t)bootinfo->bi_modules);
	     (addr_t)mod != PTOKV((addr_t)NULL); mod = (struct LOADER_MODULE*)PTOKV((addr_t)mod->mod_next)) {
		/* Ignore any non-modules */
		if (mod->mod_type != MOD_MODULE)
			continue;

		/* Got a module here; we need to relocate and load it */
		DPRINTF("module_init: module at %p-%p\n", (addr_t)mod->mod_phys_start_addr, (addr_t)mod->mod_phys_end_addr);
		errorcode_t err = module_load(mod);
		if (err != ANANAS_ERROR_NONE)
			kprintf("cannot load module %p: %u\n", mod, err);
	}
#else
	(void)module_load;
#endif
	return ANANAS_ERROR_OK;
}

INIT_FUNCTION(module_init, SUBSYSTEM_MODULE, ORDER_LAST);

/* vim:set ts=2 sw=2: */
