#ifndef RTLD_H
#define RTLD_H

#include "list.h"
#include <machine/elf.h>

struct Needed : public List<Needed>::Ptr
{
	size_t n_name_idx;
	struct Object* n_object;
};

struct ObjectListEntry : public List<ObjectListEntry>::Ptr
{
	struct Object* ol_object;
};
typedef List<ObjectListEntry> ObjectList;

/*
 * A linker object object - this is an executable, the RTLD itself or
 * any shared library we know about.
 */
struct Object : public List<Object>::Ptr
{
	addr_t o_reloc_base;		// Relocated base address
	const char* o_strtab;
	size_t o_strtab_sz;
	Elf_Sym* o_symtab;
	size_t o_symtab_sz;
	char* o_rela;
	size_t o_rela_sz;
	size_t o_rela_ent;
	Elf_Addr* o_plt_got;
	Elf_Rela* o_jmp_rela;
	size_t o_plt_rel_sz;

	const char* o_name;
	bool o_main;

	Elf_Addr o_init;
	Elf_Addr o_fini;

	const Elf_Phdr* o_phdr;
	Elf_Half o_phdr_num;

	Elf_Dyn* o_dynamic;

	// System V hashed symbols
	unsigned int o_sysv_nbucket;
	unsigned int o_sysv_nchain;
	const uint32_t* o_sysv_bucket;
	const uint32_t* o_sysv_chain;

	List<Needed> o_needed;
	ObjectList o_lookup_scope;
};

typedef List<Object> Objects;

#endif // RTLD_H
