#include <ananas/types.h>
#include <ananas/procinfo.h>
#include <ananas/elfinfo.h>
#include <fcntl.h>
#include <unistd.h>
#include <machine/param.h> // for PAGE_SIZE
#include <sys/mman.h>
#include <machine/elf.h>
#include <limits.h>
#include "lib.h"
#include "rtld.h"

/*
 * _DYNAMIC will be output by the linker and contains the contents of the
 * '.dynamic' section. (ELF spec 2-6)
 */
extern Elf_Dyn _DYNAMIC;
#pragma weak _DYNAMIC

extern "C" Elf_Addr rtld_bind_trampoline();

namespace
{

bool debug = false;
const char* ld_library_path = nullptr;
const char* ld_default_path = "/lib:/usr/lib";

#define dbg(...) (debug ? (void)printf(__VA_ARGS__) : (void)0)

// List of all objects loaded
Objects s_Objects;
ObjectList s_InitList;
ObjectList s_FiniList;

Object*
AllocateObject(const char* name)
{
	Object* obj = new Object;
	memset(obj, 0, sizeof(*obj));
	obj->o_name = strdup(name);
	s_Objects.Append(*obj);
	return obj;
}

Object*
FindLibraryByName(const char* name)
{
	for(auto& obj: s_Objects) {
		if (strcmp(obj.o_name, name) == 0)
			return &obj;
	}
	return nullptr;
}

uint32_t
CalculateHash(const char* name)
{
	uint32_t h = 0;
	for (auto p = reinterpret_cast<const uint8_t*>(name); *p != '\0'; p++) {
		h = (h << 4) + *p;
		uint32_t g = h & 0xf0000000;
		if (g != 0)
			h ^= g >> 24;
		h &= ~g;
	}
	return h;
}

inline bool
IsPowerOf2(uint32_t v)
{
	return (v & (v - 1)) == 0;
}

inline bool
IsEnvironmentVariableSet(const char* env)
{
	char* p = getenv(env);
	return p != nullptr && *p != '0';
}

static inline void
ObjectListAppend(ObjectList& ol, Object& o)
{
	// XXX this is a silly way to avoid adding duplicates
	for (const auto& entry: ol)
		if (entry.ol_object == &o)
			return;
	auto ole = new ObjectListEntry;
	ole->ol_object = &o;
	ol.Append(*ole);
}

static inline void
ObjectListPrepend(ObjectList& ol, Object& o)
{
	// XXX this is a silly way to avoid adding duplicates
	for (const auto& entry: ol)
		if (entry.ol_object == &o)
			return;
	auto ole = new ObjectListEntry;
	ole->ol_object = &o;
	ol.Prepend(*ole);
}

void
ObjectInitializeLookupScope(Object& target, Object& o, Object& main_obj)
{
	// Initially, the main executable - we always need to search first
	ObjectListAppend(target.o_lookup_scope, main_obj);

	// Then, the object itself
	ObjectListAppend(target.o_lookup_scope, o);

	// Then we need to add all NEEDED things
	for(auto& needed: o.o_needed) {
		if (needed.n_object == nullptr)
			die("%s: unreferenced NEEDED '%s' found", o.o_name, o.o_strtab + needed.n_name_idx);
		ObjectListAppend(target.o_lookup_scope, *needed.n_object);
	}

	// Repeat, for all NEEDED things - we need to look at their dependencies as well
	for(auto& needed: o.o_needed) {
		ObjectInitializeLookupScope(target, *needed.n_object, main_obj);
	}
}

// Attempts to locate a file based on a given name from a specified set of colon-separated directories
// Note: path must be PATH_MAX-bytes sized!
int
OpenFile(const char* name, const char* paths, char* dest_path)
{
	if (paths == nullptr)
		return -1;

	char tmp_path[PATH_MAX];
	for(const char* cur = paths; *cur != '\0'; /* nothing */) {
		// XXX we need range checking here!
		char* sep = strchr(cur, ':');
		if (sep != nullptr) {
			strncpy(tmp_path, cur, sep - cur);
			cur = sep + 1;
		} else {
			strcpy(tmp_path, cur);
			cur = ""; // to break loop
		}
		strcat(tmp_path, "/");
		strcat(tmp_path, name);

		// See if the file exists; that's good for now
		int fd = open(tmp_path, O_RDONLY);
		if (fd >= 0) {
			// XXX we should realpath() path here
			strcpy(dest_path, tmp_path);
			return fd;
		}
	}

	return -1;
}

} // unnamed namespace

bool find_symdef(Object& ref_obj, Elf_Addr ref_symnum, bool skip_ref_obj, Object*& def_obj, Elf_Sym*& def_sym);
const char* sym_getname(const Object& obj, unsigned int symnum);

void
parse_dynamic(Object& obj, const Elf_Dyn* dyn)
{
	for(/* nothing */; dyn->d_tag != DT_NULL; dyn++) {
		switch(dyn->d_tag) {
			case DT_STRTAB:
				obj.o_strtab = reinterpret_cast<const char*>(obj.o_reloc_base + dyn->d_un.d_ptr);
				break;
			case DT_STRSZ:
				obj.o_strtab_sz = dyn->d_un.d_val;
				break;
			case DT_SYMTAB:
				obj.o_symtab = reinterpret_cast<Elf_Sym*>(obj.o_reloc_base + dyn->d_un.d_ptr);
				break;
			case DT_RELA:
				obj.o_rela = reinterpret_cast<char*>(obj.o_reloc_base + dyn->d_un.d_ptr);
				break;
			case DT_RELASZ:
				obj.o_rela_sz = dyn->d_un.d_val;
				break;
			case DT_RELAENT:
				obj.o_rela_ent = dyn->d_un.d_val;
				break;
			case DT_SYMENT:
				obj.o_symtab_sz = dyn->d_un.d_val;
				break;
			case DT_PLTGOT:
				obj.o_plt_got = reinterpret_cast<Elf_Addr*>(obj.o_reloc_base + dyn->d_un.d_ptr);
				break;
			case DT_JMPREL:
				// XXX we assume RELA here
				obj.o_jmp_rela = reinterpret_cast<Elf_Rela*>(obj.o_reloc_base + dyn->d_un.d_ptr);
				break;
			case DT_PLTREL:
				// XXX AMD64 only supports RELA
				assert(dyn->d_un.d_val == DT_RELA);
				break;
			case DT_PLTRELSZ:
				obj.o_plt_rel_sz = dyn->d_un.d_val;
				break;
			case DT_REL:
			case DT_RELSZ:
			case DT_RELENT:
				// XXX AMD64 only uses RELA relocations; we don't
				// support anything else for now...
				panic("rel reloc unsupported");
				break;
			case DT_NEEDED: {
				auto needed = new Needed;
				memset(needed, 0, sizeof(struct Needed));
				needed->n_name_idx = dyn->d_un.d_val;
				obj.o_needed.Append(*needed);
				break;
			}
			case DT_HASH:
			{
				struct SYSV_HASH {
					uint32_t sh_nbucket;
					uint32_t sh_nchain;
					uint32_t sh_bucket;
				} *sh = reinterpret_cast<struct SYSV_HASH*>(obj.o_reloc_base + dyn->d_un.d_ptr);
				obj.o_sysv_nbucket = sh->sh_nbucket;
				obj.o_sysv_nchain = sh->sh_nchain;
				obj.o_sysv_bucket = reinterpret_cast<uint32_t*>(&sh->sh_bucket);
				obj.o_sysv_chain = obj.o_sysv_bucket + sh->sh_nbucket;
				break;
			}
			case DT_INIT:
				obj.o_init = obj.o_reloc_base + dyn->d_un.d_ptr;
				break;
			case DT_FINI:
				obj.o_fini = obj.o_reloc_base + dyn->d_un.d_ptr;
				break;
			case DT_INIT_ARRAY:
				die("%s: DT_INIT_ARRAY unsupported", obj.o_name);
			case DT_FINI_ARRAY:
				die("%s: DT_FINI_ARRAY unsupported", obj.o_name);
			case DT_NULL:
			case DT_DEBUG:
				break;
#if 0
			default:
				printf(">> parse_dynamic: unrecognized tag=%d\n", dyn->d_tag);
#endif
		}
	}
}

void
process_relocations_rela(Object& obj)
{
	dbg("%s: process_relocations_rela()\n", obj.o_name);
	char* rela_ptr = obj.o_rela;
	char* rela_end = rela_ptr + obj.o_rela_sz;
	for (/* nothing */; rela_ptr < rela_end; rela_ptr += obj.o_rela_ent) {
		auto& rela = *reinterpret_cast<Elf_Rela*>(rela_ptr);
		auto& v64 = *reinterpret_cast<uint64_t*>(obj.o_reloc_base + rela.r_offset);
		int r_type = ELF_R_TYPE(rela.r_info);

		// First phase: look up for the type where we need to do so
		Elf_Addr sym_addr = 0;
		switch (r_type) {
			case R_X86_64_64:
			case R_X86_64_GLOB_DAT: {
				uint32_t symnum = ELF_R_SYM(rela.r_info);
				Object* def_obj;
				Elf_Sym* def_sym;
				if (!find_symdef(obj, symnum, false, def_obj, def_sym))
					die("%s: symbol '%s' not found", obj.o_name, sym_getname(obj, symnum));
				sym_addr = def_obj->o_reloc_base + def_sym->st_value;
				break;
			}
		}

		// Second phase: fill out values
		switch (r_type) {
			case R_X86_64_64:
				v64 = sym_addr + rela.r_addend;
				break;
			case R_X86_64_RELATIVE:
				v64 = obj.o_reloc_base + rela.r_addend;
				break;
			case R_X86_64_GLOB_DAT: {
				v64 = sym_addr;
				break;
			}
			case R_X86_64_COPY:
				// Do not resolve COPY relocations here; these must be deferred until we have all
				// objects in place
				if (!obj.o_main)
					die("%s: unexpected copy relocation", obj.o_name);
				break;
			default:
				die("%s: unsupported rela type %d (offset %x info %d add %x)", obj.o_name, r_type, rela.r_offset, rela.r_info, rela.r_addend);
		}
	}
}

void
process_relocations_plt(Object& obj)
{
	char* rela_ptr = (char*)obj.o_jmp_rela;
	char* rela_end = rela_ptr + obj.o_plt_rel_sz;
	for (/* nothing */; rela_ptr < rela_end; rela_ptr += obj.o_rela_ent) {
		auto& rela = *reinterpret_cast<Elf_Rela*>(rela_ptr);
		auto& v64 = *reinterpret_cast<uint64_t*>(obj.o_reloc_base + rela.r_offset);
		switch(ELF_R_TYPE(rela.r_info)) {
			case R_X86_64_JUMP_SLOT:
				v64 += obj.o_reloc_base;
				break;
			default:
				die("%s: unsuppored rela type %d in got", obj.o_name, ELF_R_TYPE(rela.r_info));
		}
	}
}

void
process_relocations_copy(Object& obj)
{
	/*
	 * COPY relocations are only valid in the main executable, and are
	 * executed at the end to ensure all dependencies are loaded.
	 *
	 * Note that we must lookup symbols with 'skip_ref_obj = true' to
	 * avoid finding the symbols in the main executable (these are the
	 * symbol values we intend to fill)
	 */
	dbg("process_relocations_copy for '%s'\n", obj.o_name);
	char* rela_ptr = obj.o_rela;
	char* rela_end = rela_ptr + obj.o_rela_sz;
	for (/* nothing */; rela_ptr < rela_end; rela_ptr += obj.o_rela_ent) {
		auto& rela = *reinterpret_cast<Elf_Rela*>(rela_ptr);
		if (ELF_R_TYPE(rela.r_info) != R_X86_64_COPY)
			continue;

		uint32_t symnum = ELF_R_SYM(rela.r_info);
		Object* def_obj;
		Elf_Sym* def_sym;
		if (!find_symdef(obj, symnum, true, def_obj, def_sym))
			die("%s: symbol '%s' not found", obj.o_name, sym_getname(obj, symnum));

		auto src_ptr = reinterpret_cast<char*>(def_obj->o_reloc_base + def_sym->st_value);
		auto dst_ptr = reinterpret_cast<char*>(obj.o_reloc_base + rela.r_offset);
		dbg("copy(): %p -> %p (sz=%d)\n", src_ptr, dst_ptr, def_sym->st_size);
		memcpy(dst_ptr, src_ptr, def_sym->st_size);
	}
}

void
rtld_relocate(addr_t base)
{
	// Place the object on the stack as we may not be able to access global data yet
	Object temp_obj;
	memset(&temp_obj, 0, sizeof(temp_obj));
	temp_obj.o_name = "ld-ananas.so";

	const Elf_Dyn* dyn = reinterpret_cast<const Elf_Dyn*>(base + reinterpret_cast<addr_t>(&_DYNAMIC));
	temp_obj.o_reloc_base = base;
	parse_dynamic(temp_obj, dyn);
	process_relocations_rela(temp_obj);

	// Move our relocatable object to the object chain
	Object* rtld_obj = AllocateObject(temp_obj.o_name);
	memcpy(rtld_obj, &temp_obj, sizeof(temp_obj));
}

void
process_phdr(Object& obj, const Elf_Phdr* phdr, size_t phdr_num_entries)
{
	dbg("%s: process_phdr()\n", obj.o_name);
	for (size_t n = 0; n < phdr_num_entries; n++, phdr++) {
		switch (phdr->p_type) {
		case PT_NULL:
		case PT_LOAD:
			break;
		case PT_DYNAMIC:
			parse_dynamic(obj, reinterpret_cast<Elf_Dyn*>(obj.o_reloc_base + phdr->p_vaddr));
			break;
		case PT_NOTE:
			// XXX We should parse the .note here ...
			break;
#if 0
		default:
			printf("process_phdr(): unrecognized type %d\n", phdr->p_type);
			break;
#endif
		}
	}
}

// XXX This is amd64-specific
void
setup_got(Object& obj)
{
	if (obj.o_plt_got == nullptr)
		return; // nothing to set up

	obj.o_plt_got[1] = reinterpret_cast<Elf_Addr>(&obj);
	obj.o_plt_got[2] = reinterpret_cast<Elf_Addr>(&rtld_bind_trampoline);

	dbg("%s: rtld_bind_trampoline: obj %p ptr %p\n", obj.o_name, obj.o_plt_got[1], obj.o_plt_got[2]);
}

bool
sym_matches(const Object& obj, const Elf_Sym& sym, const char* name)
{
	switch(ELF_ST_TYPE(sym.st_info)) {
		case STT_NOTYPE:
		case STT_FUNC:
			// If the symbol isn't actually declared here, do not match it - this
			// prevents functions from being matched in the .dynsym table of the
			// finder
			if (sym.st_shndx == SHN_UNDEF)
				return false;
	}

	const char* sym_name = obj.o_strtab + sym.st_name;
	return strcmp(sym_name, name) == 0;
}


const char*
sym_getname(const Object& obj, unsigned int symnum)
{
	Elf_Sym& sym = obj.o_symtab[symnum];
	return obj.o_strtab + sym.st_name;
}

// ref_... is the reference to the symbol we need to look up; on success,
// def_... will contain the definition of the symbol
bool
find_symdef(Object& ref_obj, Elf_Addr ref_symnum, bool skip_ref_obj, Object*& def_obj, Elf_Sym*& def_sym)
{
	Elf_Sym& ref_sym = ref_obj.o_symtab[ref_symnum];
	const char* ref_name = ref_obj.o_strtab + ref_sym.st_name;

	// If this symbol is local, we can use it as-is
	if(ELF_ST_BIND(ref_sym.st_info) == STB_LOCAL) {
		def_obj = &ref_obj;
		def_sym = &ref_sym;
		return true;
	}

	// Not local, need to look it up
	uint32_t hash = CalculateHash(ref_name);
	for (const auto& entry: ref_obj.o_lookup_scope) {
		auto& obj = *entry.ol_object;
		if (obj.o_sysv_bucket == nullptr)
			continue;
		if (skip_ref_obj && &obj == &ref_obj)
			continue;

		unsigned int symnum = obj.o_sysv_bucket[hash % obj.o_sysv_nbucket];
		while(symnum != STN_UNDEF) {
			if (symnum >= obj.o_sysv_nchain)
				break;

			Elf_Sym& sym = obj.o_symtab[symnum];
			const char* sym_name = obj.o_strtab + sym.st_name;
			if (sym_matches(obj, sym, ref_name)) {
				def_obj = &obj;
				def_sym = &sym;
				dbg("find_symdef(): '%s' defined in %s @ %p\n", sym_name, obj.o_name, sym.st_value);
				return true;
			}

			symnum = obj.o_sysv_chain[symnum];
		}
	}

	return false;
}

extern "C"
Elf_Addr
rtld_bind(Object* obj, size_t offs)
{
	const Elf_Rela& rela = obj->o_jmp_rela[offs];

	dbg("%s: rtld_bind(): rela: %p, offset %p sym %d type %d\n",
	 obj->o_name, &rela, rela.r_offset, ELF_R_SYM(rela.r_info), ELF_R_TYPE(rela.r_info));

	Object* ref_obj;
	Elf_Sym* ref_sym;
	uint32_t symnum = ELF_R_SYM(rela.r_info);
	if (!find_symdef(*obj, symnum, false, ref_obj, ref_sym))
		die("%s: symbol '%s' not found", obj->o_name, sym_getname(*obj, symnum));
	Elf_Addr target = ref_obj->o_reloc_base + ref_sym->st_value;

	dbg("%s: sym '%s' found in %s (%p)\n", obj->o_name, sym_getname(*obj, symnum), ref_obj->o_name, target);

	/* Alter the jump slot so we do not have to go via the RTLD anymore */
	Elf_Addr* jmp_slot = reinterpret_cast<Elf_Addr*>(obj->o_reloc_base + rela.r_offset);
	*jmp_slot = target;
	return target;
}

int
open_library(const char* name, char* path)
{
	int fd;

	// XXX for now we hardncode the path and don't care about security
	fd = OpenFile(name, ld_library_path, path);
	if (fd >= 0)
		return fd;

	return OpenFile(name, ld_default_path, path);
}

static bool
elf_verify_header(const Elf_Ehdr& ehdr)
{
	/* Perform basic ELF checks; must be statically-linked executable */
	if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr.e_ident[EI_MAG2] != ELFMAG2 || ehdr.e_ident[EI_MAG3] != ELFMAG3)
		return false;
	if (ehdr.e_ident[EI_VERSION] != EV_CURRENT)
		return false;

	/* XXX This specifically checks for amd64 at the moment */
	if (ehdr.e_ident[EI_CLASS] != ELFCLASS64)
		return false;
	if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB)
		return false;
	if (ehdr.e_machine != EM_X86_64)
		return false;

	// We need a program header here as it contains the things we need to load
	if (ehdr.e_phnum == 0)
		return false;
	if (ehdr.e_phentsize < sizeof(Elf64_Phdr))
		return false;

	// We'll only map the first page into memory; ensure everything we need is in there
	if (ehdr.e_phentsize != sizeof(Elf_Phdr))
		return false;
	if (ehdr.e_phoff + ehdr.e_phnum * ehdr.e_phentsize >= PAGE_SIZE)
		return false;

	return true;
}

static inline addr_t
round_down_page(addr_t addr)
{
	return addr & ~(PAGE_SIZE - 1);
}

static inline addr_t
round_up_page(addr_t addr)
{
	if ((addr & (PAGE_SIZE - 1)) == 0)
		return addr;

	return (addr | (PAGE_SIZE - 1)) + 1;
}

static inline int
elf_prot_from_phdr(const Elf_Phdr& phdr)
{
	int prot = 0;
	if (phdr.p_flags & PF_X)
		prot |= PROT_EXEC;
	if (phdr.p_flags & PF_W)
		prot |= PROT_WRITE;
	if (phdr.p_flags & PF_R)
		prot |= PROT_READ;
	return prot;
}

static inline int
elf_flags_from_phdr(const Elf_Phdr& phdr)
{
	int flags = MAP_PRIVATE;
	return flags;
}

Object*
map_object(int fd, const char* name)
{
	// Map the first page into memory - this makes things a lot easier to process
	void* first = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_SHARED, fd, 0);
	if (first == (void*)-1)
		die("%s: mmap of header failed", name);

	auto& ehdr = *static_cast<const Elf_Ehdr*>(first);
	if (!elf_verify_header(ehdr))
		die("%s: not a (valid) ELF file", name);

	// Determine the high and low addresses of the object
	addr_t v_start = (addr_t)-1;
	addr_t v_end = 0;
	{
		auto phdr = reinterpret_cast<const Elf_Phdr*>(static_cast<char*>(first) + ehdr.e_phoff);
		for (unsigned int n = 0; n < ehdr.e_phnum; n++, phdr++) {
			switch(phdr->p_type) {
				case PT_LOAD:
					addr_t last_addr = phdr->p_vaddr + phdr->p_memsz;
					if (v_start > phdr->p_vaddr)
						v_start = phdr->p_vaddr;
					if (last_addr > v_end)
						v_end = last_addr;
					break;
			}
		}
		v_start = round_up_page(v_start);
		v_end = round_up_page(v_end);
	}
	if (v_start != 0)
		die("%s: base not a zero", name);

	// First, reserve the entire range of memory - we'll update the permissions
	// and actual content later, but this ensures we'll have a continous range
	addr_t base = reinterpret_cast<addr_t>(mmap(reinterpret_cast<void*>(v_start), v_end - v_start, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
	if (base == (addr_t)-1)
		die("%s: unable to allocate range %p-%p", name, v_start, v_end);
	dbg("%s: reserved %p-%p\n", name, base, base + (v_end - v_start));

	// Now walk through the program headers and actually map them
	{
		auto phdr = reinterpret_cast<const Elf_Phdr*>(static_cast<char*>(first) + ehdr.e_phoff);
		for (unsigned int n = 0; n < ehdr.e_phnum; n++, phdr++) {
			if(phdr->p_type != PT_LOAD)
				continue;

			addr_t v_base = round_down_page(phdr->p_vaddr);
			addr_t v_len = round_up_page(phdr->p_filesz);
			off_t offset = round_down_page(phdr->p_offset);
			int prot = elf_prot_from_phdr(*phdr);
			int flags = elf_flags_from_phdr(*phdr) | MAP_FIXED;

			dbg("%s: remapping %p-%p from offset %d v_len %d\n", name, base + v_base, (base + v_base + v_len) - 1, static_cast<int>(offset), v_len);
			addr_t p = reinterpret_cast<addr_t>(mmap(reinterpret_cast<void*>(base + v_base), v_len, prot, flags, fd, offset));
			if (p == (addr_t)-1)
				die("%s: unable to map %p-%p", name, base + v_base, base + v_base + v_end);

			if (phdr->p_filesz >= phdr->p_memsz)
				continue;

			// The kernel will silent truncate the request if we
			// extend more than a partial page - to avoid this, add
			// an extra mapping covering the parts that fall
			// outside of the scope
			addr_t v_extra = round_up_page(phdr->p_vaddr + phdr->p_filesz);
			addr_t v_extra_len = round_up_page(phdr->p_vaddr + phdr->p_memsz - v_extra);
			dbg("%s: mapping zero data to %p..%p\n", name, base + v_extra, base + v_extra + v_extra_len);
			p = reinterpret_cast<addr_t>(mmap(reinterpret_cast<void*>(base + v_extra), v_extra_len, prot, flags, -1, 0));
			if (p == (addr_t)-1)
				die("%s: unable to map %p-%p", name, base + v_extra, base + v_extra + v_extra_len);

			// Zero out everything beyond what is file-mapped XXX cope with read-only mappings here XXX the kernel should do this
			memset(reinterpret_cast<void*>(p), 0, v_extra_len);
		}
	}

	munmap(first, PAGE_SIZE);

	Object* obj = AllocateObject(name);
	obj->o_reloc_base = base;

	process_phdr(*obj, reinterpret_cast<const Elf_Phdr*>(static_cast<char*>(first) + ehdr.e_phoff), ehdr.e_phnum);
	if (obj->o_sysv_nbucket == 0 || obj->o_sysv_nchain == 0)
		die("%s: hash not present or unusable", name);

	setup_got(*obj);
	return obj;
}

void
dump_libs()
{
	for(const auto& object: s_Objects) {
		if (object.o_main)
			continue;
		printf("%s => 0x%p\n", object.o_name, object.o_reloc_base);
	}
}

void
dump_init_fini()
{
	printf("Init funcs:\n");
	for(const auto& ole: s_InitList) {
		Object& o = *ole.ol_object;
		printf("%s (%p)\n", o.o_name, o.o_init);
	}

	printf("Fini funcs:\n");
	for(const auto& ole: s_FiniList) {
		Object& o = *ole.ol_object;
		printf("%s (%p)\n", o.o_name, o.o_fini);
	}
}

void
dump_lookup_scope(Object& obj)
{
	printf("%s (%p)\n", obj.o_name, obj.o_reloc_base);
	for(const auto& ole: obj.o_lookup_scope) {
		Object& o = *ole.ol_object;
		printf("  %s (%p)\n", o.o_name, o.o_reloc_base);
	}
}

void
process_init_fini_funcs(Object& obj)
{
	// First, dive into all NEEDED objects
	for(auto& needed: obj.o_needed) {
		process_init_fini_funcs(*needed.n_object);
	}

	// And add the current object, if it has an init function
	if (obj.o_init)
		ObjectListAppend(s_InitList, obj);
	if (obj.o_fini)
		ObjectListPrepend(s_FiniList, obj);
}

void
run_init_funcs()
{
	for(const auto& ole: s_InitList) {
		Object& o = *ole.ol_object;
		typedef void (*FuncPtr)();
		((FuncPtr)(o.o_init))();
	}
}

void
run_fini_funcs()
{
	for(const auto& ole: s_FiniList) {
		Object& o = *ole.ol_object;
		typedef void (*FuncPtr)();
		((FuncPtr)(o.o_fini))();
	}
}

extern "C"
Elf_Addr
rtld(void* procinfo, struct ANANAS_ELF_INFO* ei, addr_t* exit_func)
{
	/*
	 * We are a dynamic executable, which means the kernel just chose a
	 * virtual address to place us. We have to adjust our dynamic
	 * relocations to match - we have been loaded at ei_interpreter_base.
	 */
	rtld_relocate(ei->ei_interpreter_base);

	// Get access to the command line and environment; we need this to
	// find our program name and any LD_... settings
	InitializeProcessInfo(procinfo);
	debug |= IsEnvironmentVariableSet("LD_DEBUG");

	// Now, locate our executable and process it
	auto main_obj = AllocateObject(GetProgName());
	main_obj->o_main = true;
	process_phdr(*main_obj, reinterpret_cast<const Elf_Phdr*>(ei->ei_phdr), ei->ei_phdr_entries);
	setup_got(*main_obj);

	// Handle LD_LIBRARY_PATH - we'll be loading extra things from here on
	ld_library_path = getenv("LD_LIBRARY_PATH");

	/*
	 * Walk through all objects and process their needed parts. We use the
	 * fact that things are added in-order to s_Objects here.
	 *
	 * Do not use _SAFE here, we want to re-evaluate the next object every
	 * loop iteration.
	 */
	for(auto& obj: s_Objects) {
		for(auto& needed: obj.o_needed) {
			const char* name = obj.o_strtab + needed.n_name_idx;
			char path[PATH_MAX];
			int fd = open_library(name, path);
			if (fd < 0)
				die("unable to open library '%s', aborting\n", name);

			// XXX we should check whether fd points to something we already know,
			// but we'd need fstat() for that...
			Object* found_obj = FindLibraryByName(path);
			if (found_obj != nullptr) {
				close(fd);
				if (found_obj == &obj)
					die("%s: depends on itself", name);
				needed.n_object = found_obj;
				continue;
			}


			Object* new_obj = map_object(fd, path);
			if (new_obj == nullptr)
				die("cannot load library '%s', aborting\n", name);
			needed.n_object = new_obj;
		}
	}

	for(auto& obj: s_Objects) {
		ObjectInitializeLookupScope(obj, obj, *main_obj);
		if (!debug)
			continue;
		printf("lookup scope for '%s'\n", obj.o_name);
		dump_lookup_scope(obj);
	}

	// Process all relocations
	for(auto& obj: s_Objects) {
		process_relocations_rela(obj);
		process_relocations_plt(obj);
	}
	process_relocations_copy(*main_obj);
	if (IsEnvironmentVariableSet("LD_LDD")) {
		dump_libs();
		exit(0);
	}

	// Create list of init/fini functions and run them prior to the executable itself
	process_init_fini_funcs(*main_obj);
	run_init_funcs();

	/*
	 * Yield the address of our executable - this ensures our stack frame
	 * gets cleaned up (we don't need anything we allocated locally here,
	 * so best get rid of it so our executable starts with a clean slate)
	 */
	dbg("transferring control to %p\n", ei->ei_entry);
	*exit_func = reinterpret_cast<addr_t>(&run_fini_funcs);
	return ei->ei_entry;
}
