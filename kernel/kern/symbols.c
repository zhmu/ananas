#include <ananas/types.h>
#include <ananas/symbols.h>
#include <ananas/bootinfo.h>
#include <ananas/lib.h>
#include <ananas/vm.h>
#include <machine/param.h>		/* for PAGE_SIZE */
#include <elf.h>

static void* sym_tab = NULL;
static void* str_tab = NULL;

typedef Elf32_Sym Elf_Sym;

static inline void*
symbols_map_addr(addr_t addr, size_t len)
{
	int offset = addr & (PAGE_SIZE - 1);
	/* Round addr down to a page */
	if (offset) {
		len += offset;
		addr &= ~(PAGE_SIZE - 1);
	}
	/* Round len up to a page */
	if (len & (PAGE_SIZE - 1))
		len = (len | (PAGE_SIZE - 1)) + 1;
	return vm_map_kernel(addr, len, VM_FLAG_READ) + offset;
}

int
symbol_resolve_addr(addr_t addr, struct SYMBOL* s)
{
	Elf_Sym* sym = sym_tab;
	for (unsigned int i = 0; i < bootinfo->bi_symtab_size; i += sizeof(Elf_Sym), sym++) {
		if (addr < sym->st_value ||
		    addr >= sym->st_value + sym->st_size)
			continue;
		s->s_addr = sym->st_value;
		s->s_name = str_tab + sym->st_name;
		s->s_flags = 0; /* TODO */
		return 1;
	}
	return 0;
}

int
symbol_resolve_name(const char* name, struct SYMBOL* s)
{
	Elf_Sym* sym = sym_tab;
	for (unsigned int i = 0; i < bootinfo->bi_symtab_size; i += sizeof(Elf_Sym), sym++) {
		if (strcmp(str_tab + sym->st_name, name) != 0)
			continue;
		s->s_addr = sym->st_value;
		s->s_name = str_tab + sym->st_name;
		s->s_flags = 0; /* TODO */
		return 1;
	}
	return 0;
}

void
symbols_init()
{
	/* Reject any missing bootinfo field */
	if (bootinfo == NULL)
		return;
	if (bootinfo->bi_symtab_addr == 0 || bootinfo->bi_symtab_size == 0)
		return;
	if (bootinfo->bi_strtab_addr == 0 || bootinfo->bi_strtab_size == 0)
		return;

	sym_tab = symbols_map_addr(bootinfo->bi_symtab_addr, bootinfo->bi_symtab_size);
	str_tab = symbols_map_addr(bootinfo->bi_strtab_addr, bootinfo->bi_strtab_size);
}

/* vim:set ts=2 sw=2: */
