#ifndef RTLD_H
#define RTLD_H

#include "list.h"
#include "elf-types.h"

LIST_DEFINE(Objects, struct Object);

struct Needed
{
	LIST_FIELDS(struct Needed);
	size_t n_name_idx;
	struct Object* n_object;
};
LIST_DEFINE(NeededList, struct Needed);

struct ObjectListEntry
{
	LIST_FIELDS(struct ObjectListEntry);
	struct Object* ol_object;
};
LIST_DEFINE(ObjectList, struct ObjectListEntry);

/*
 * A linker object object - this is an executable, the RTLD itself or
 *any shared library we know about.
 */
struct Object
{
	LIST_FIELDS(struct Object);

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

	// System V hashed symbols
	unsigned int o_sysv_nbucket;
	unsigned int o_sysv_nchain;
	const uint32_t* o_sysv_bucket;
	const uint32_t* o_sysv_chain;

	struct NeededList o_needed;
	struct ObjectList o_lookup_scope;
};

#endif // RTLD_H
