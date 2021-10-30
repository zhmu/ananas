/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <machine/param.h> // for PAGE_SIZE
#include <sys/mman.h>
#include <machine/elf.h>
#include <link.h>
#include <limits.h>
#include "lib.h"
#include "rtld.h"

extern "C" Elf_Dyn* rtld_dynamic();
extern "C" Elf_Addr rtld_bind_trampoline();

#define SYMDEF_FLAG_SKIP_REF_OBJ 1

namespace
{
    bool debug = false;
    bool summary = false;
    struct r_debug r_debugstate;
    const char* ld_library_path = nullptr;
    const char* ld_default_path = "/lib:/usr/lib";

#define dbg(...) (debug ? (void)printf(__VA_ARGS__) : (void)0)
#define sum(...) (summary ? (void)printf(__VA_ARGS__) : (void)0)

    int main_argc;
    char** main_argv;
    char** main_envp;

    // List of all objects loaded
    Objects s_Objects;
    ObjectList s_InitList;
    ObjectList s_FiniList;

    Object* AllocateObject(const char* name)
    {
        Object* obj = new Object;
        memset(obj, 0, sizeof(*obj));
        obj->o_name = strdup(name);
        s_Objects.push_back(*obj);
        return obj;
    }

    template<typename Func>
    Object* FindLibrary(Func func)
    {
        for (auto& obj : s_Objects) {
            if (func(obj))
                return &obj;
        }
        return nullptr;
    }

    uint32_t CalculateHash(const char* name)
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

    inline bool IsPowerOf2(uint32_t v) { return (v & (v - 1)) == 0; }

    inline bool IsEnvironmentVariableSet(const char* env)
    {
        char* p = getenv(env);
        return p != nullptr && *p != '0';
    }

    static inline void ObjectListAppend(ObjectList& ol, Object& o)
    {
        // XXX this is a silly way to avoid adding duplicates
        for (const auto& entry : ol)
            if (entry.ol_object == &o)
                return;
        auto ole = new ObjectListEntry;
        ole->ol_object = &o;
        ol.push_back(*ole);
    }

    static inline void ObjectListPrepend(ObjectList& ol, Object& o)
    {
        // XXX this is a silly way to avoid adding duplicates
        for (const auto& entry : ol)
            if (entry.ol_object == &o)
                return;
        auto ole = new ObjectListEntry;
        ole->ol_object = &o;
        ol.push_front(*ole);
    }

    void ObjectInitializeLookupScope(Object& target, Object& o, Object& main_obj)
    {
        // Initially, the main executable - we always need to search first
        ObjectListAppend(target.o_lookup_scope, main_obj);

        // Then, the object itself
        ObjectListAppend(target.o_lookup_scope, o);

        // Then we need to add all NEEDED things
        for (auto& needed : o.o_needed) {
            if (needed.n_object == nullptr)
                die("%s: unreferenced NEEDED '%s' found", o.o_name, o.o_strtab + needed.n_name_idx);
            ObjectListAppend(target.o_lookup_scope, *needed.n_object);
        }

        // Repeat, for all NEEDED things - we need to look at their dependencies as well
        for (auto& needed : o.o_needed) {
            ObjectInitializeLookupScope(target, *needed.n_object, main_obj);
        }
    }

    // Attempts to locate a file based on a given name from a specified set of colon-separated
    // directories Note: path must be PATH_MAX-bytes sized!
    int OpenFile(const char* name, const char* paths, char* dest_path)
    {
        if (paths == nullptr)
            return -1;

        char tmp_path[PATH_MAX];
        for (const char* cur = paths; *cur != '\0'; /* nothing */) {
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

    /*
     * Used by the debugger to place a breakpoint when we change share library
     * information. It is presented to the debugger via the r_debugstate structure
     * obtained from DT_DEBUG.
     */
    void r_debug_state(struct r_debug* rd, struct link_map* m) { __asm __volatile("nop"); }

    /* Used to maintain r_debug */
    void linkmap_add(Object& obj)
    {
        auto lm = new link_map;
        memset(lm, 0, sizeof(*lm));
        lm->l_addr = obj.o_reloc_base;
        lm->l_name = obj.o_name;
        lm->l_ld = obj.o_dynamic;

        if (r_debugstate.r_map != nullptr) {
            // Find the last entry
            auto last = r_debugstate.r_map;
            while (last->l_next != nullptr)
                last = last->l_next;

            // Append after the last entry
            last->l_next = lm;
            lm->l_prev = last;
        } else {
            // No entries - add us as the last one
            r_debugstate.r_map = lm;
        }
    }

} // unnamed namespace

bool find_symdef(
    Object& ref_obj, Elf_Addr ref_symnum, int flags, Object*& def_obj, Elf_Sym*& def_sym);
const char* sym_getname(const Object& obj, unsigned int symnum);

void parse_dynamic(Object& obj)
{
    auto dyn = obj.o_dynamic;
    for (/* nothing */; dyn->d_tag != DT_NULL; dyn++) {
        dbg("%s: dyn tag %d\n", obj.o_name, dyn->d_tag);
        switch (dyn->d_tag) {
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
                obj.o_rela = reinterpret_cast<Elf_Rela*>(obj.o_reloc_base + dyn->d_un.d_ptr);
                break;
            case DT_RELASZ:
                assert(dyn->d_un.d_val % sizeof(Elf_Rela) == 0);
                obj.o_rela_count = dyn->d_un.d_val / sizeof(Elf_Rela);
                break;
            case DT_RELAENT:
                assert(dyn->d_un.d_val == sizeof(Elf_Rela));
                break;
            case DT_SYMENT:
                assert(dyn->d_un.d_val == sizeof(Elf_Sym));
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
                assert(dyn->d_un.d_val % sizeof(Elf_Rela) == 0);
                obj.o_plt_rel_count = dyn->d_un.d_val / sizeof(Elf_Rela);
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
                obj.o_needed.push_back(*needed);
                break;
            }
            case DT_HASH: {
                struct SYSV_HASH {
                    uint32_t sh_nbucket;
                    uint32_t sh_nchain;
                    uint32_t sh_bucket;
                }* sh = reinterpret_cast<struct SYSV_HASH*>(obj.o_reloc_base + dyn->d_un.d_ptr);
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
            case DT_DEBUG:
                dyn->d_un.d_ptr = (Elf_Addr)&r_debugstate;
                break;
            case DT_INIT_ARRAY:
                obj.o_init_array = obj.o_reloc_base + dyn->d_un.d_ptr;
                break;
            case DT_FINI_ARRAY:
                obj.o_fini_array = obj.o_reloc_base + dyn->d_un.d_ptr;
                break;
            case DT_INIT_ARRAYSZ:
                obj.o_init_array_size = dyn->d_un.d_val / sizeof(Elf_Addr);
                break;
            case DT_FINI_ARRAYSZ:
                obj.o_fini_array_size = dyn->d_un.d_val / sizeof(Elf_Addr);
                break;
            case DT_NULL:
                break;
#if 0
			default:
				printf(">> parse_dynamic: unrecognized tag=%d\n", dyn->d_tag);
#endif
        }
    }
}

void process_relocations_rela(Object& obj)
{
    dbg("%s: process_relocations_rela()\n", obj.o_name);
    auto rela = obj.o_rela;
    for (size_t n = 0; n < obj.o_rela_count; n++, rela++) {
        auto& v64 = *reinterpret_cast<uint64_t*>(obj.o_reloc_base + rela->r_offset);
        int r_type = ELF_R_TYPE(rela->r_info);

        // First phase: look up for the type where we need to do so
        Elf_Addr sym_addr = 0;
        switch (r_type) {
            case R_X86_64_64:
            case R_X86_64_GLOB_DAT: {
                uint32_t symnum = ELF_R_SYM(rela->r_info);
                Object* def_obj;
                Elf_Sym* def_sym;
                if (!find_symdef(obj, symnum, 0, def_obj, def_sym))
                    die("%s: symbol '%s' not found", obj.o_name, sym_getname(obj, symnum));
                sym_addr = def_obj->o_reloc_base + def_sym->st_value;
                break;
            }
        }

        // Second phase: fill out values
        switch (r_type) {
            case R_X86_64_64:
                v64 = sym_addr + rela->r_addend;
                break;
            case R_X86_64_RELATIVE:
                v64 = obj.o_reloc_base + rela->r_addend;
                break;
            case R_X86_64_GLOB_DAT: {
                v64 = sym_addr;
                break;
            }
#if 0
			case R_X86_64_DTPMOD64: {
				// v64 += tlsindex;
				break;
			}
#endif
            case R_X86_64_COPY:
                // Do not resolve COPY relocations here; these must be deferred until we have all
                // objects in place
                if (!obj.o_main)
                    die("%s: unexpected copy relocation", obj.o_name);
                break;
            default:
                printf(
                    "offs %p (base %p) r_type %x sym_addr %p\n", rela, obj.o_rela, r_type,
                    sym_addr);
                die("%s: unsupported rela type %d (offset %x info %d add %x)", obj.o_name, r_type,
                    rela->r_offset, rela->r_info, rela->r_addend);
        }
    }
}

void process_relocations_plt(Object& obj)
{
    auto rela = obj.o_jmp_rela;
    for (size_t n = 0; n < obj.o_plt_rel_count; n++, rela++) {
        auto& v64 = *reinterpret_cast<uint64_t*>(obj.o_reloc_base + rela->r_offset);
        switch (ELF_R_TYPE(rela->r_info)) {
            case R_X86_64_JUMP_SLOT:
                v64 += obj.o_reloc_base;
                break;
            default:
                die("%s: unsuppored rela type %d in got", obj.o_name, ELF_R_TYPE(rela->r_info));
        }
    }
}

void process_relocations_copy(Object& obj)
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
    auto rela = obj.o_rela;
    for (size_t n = 0; n < obj.o_rela_count; n++, rela++) {
        if (ELF_R_TYPE(rela->r_info) != R_X86_64_COPY)
            continue;

        uint32_t symnum = ELF_R_SYM(rela->r_info);
        Object* def_obj;
        Elf_Sym* def_sym;
        if (!find_symdef(obj, symnum, SYMDEF_FLAG_SKIP_REF_OBJ, def_obj, def_sym))
            die("%s: symbol '%s' not found", obj.o_name, sym_getname(obj, symnum));

        auto src_ptr = reinterpret_cast<char*>(def_obj->o_reloc_base + def_sym->st_value);
        auto dst_ptr = reinterpret_cast<char*>(obj.o_reloc_base + rela->r_offset);
        dbg("copy(): %p -> %p (sz=%d)\n", src_ptr, dst_ptr, def_sym->st_size);
        memcpy(dst_ptr, src_ptr, def_sym->st_size);
    }
}

void rtld_relocate(uintptr_t base)
{
    // Place the object on the stack as we may not be able to access global data yet
    Object temp_obj;
    memset(&temp_obj, 0, sizeof(temp_obj));
    temp_obj.o_name = "ld-ananas.so";

    temp_obj.o_reloc_base = base;
    temp_obj.o_dynamic = rtld_dynamic();
    parse_dynamic(temp_obj);
    process_relocations_rela(temp_obj);

    // Move our relocatable object to the object chain
    Object* rtld_obj = AllocateObject(temp_obj.o_name);
    memcpy(rtld_obj, &temp_obj, sizeof(temp_obj));

    // Initialize r_debug state
    r_debugstate.r_brk = &r_debug_state;
    r_debugstate.r_status = r_debug::RT_CONSISTENT;
}

void process_phdr(Object& obj)
{
    const Elf_Phdr* phdr = obj.o_phdr;
    Elf_Half phdr_num_entries = obj.o_phdr_num;

    dbg("%s: process_phdr()\n", obj.o_name);
    for (size_t n = 0; n < phdr_num_entries; n++, phdr++) {
        switch (phdr->p_type) {
            case PT_NULL:
            case PT_LOAD:
                break;
            case PT_DYNAMIC:
                obj.o_dynamic = reinterpret_cast<Elf_Dyn*>(obj.o_reloc_base + phdr->p_vaddr);
                parse_dynamic(obj);
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
void setup_got(Object& obj)
{
    if (obj.o_plt_got == nullptr)
        return; // nothing to set up

    obj.o_plt_got[1] = reinterpret_cast<Elf_Addr>(&obj);
    obj.o_plt_got[2] = reinterpret_cast<Elf_Addr>(&rtld_bind_trampoline);

    dbg("%s: rtld_bind_trampoline: obj %p ptr %p\n", obj.o_name, obj.o_plt_got[1],
        obj.o_plt_got[2]);
}

bool sym_matches(const Object& obj, const Elf_Sym& sym, const char* name)
{
    switch (ELF_ST_TYPE(sym.st_info)) {
        case STT_NOTYPE:
        case STT_FUNC:
        case STT_OBJECT:
        case STT_COMMON:
            // If the symbol isn't actually declared here, do not match it - this
            // prevents functions from being matched in the .dynsym table of the
            // finder
            if (sym.st_shndx == SHN_UNDEF)
                return false;
    }

    const char* sym_name = obj.o_strtab + sym.st_name;
    return strcmp(sym_name, name) == 0;
}

const char* sym_getname(const Object& obj, unsigned int symnum)
{
    Elf_Sym& sym = obj.o_symtab[symnum];
    return obj.o_strtab + sym.st_name;
}

// ref_... is the reference to the symbol we need to look up; on success,
// def_... will contain the definition of the symbol
bool find_symdef(
    Object& ref_obj, Elf_Addr ref_symnum, int flags, Object*& def_obj, Elf_Sym*& def_sym)
{
    Elf_Sym& ref_sym = ref_obj.o_symtab[ref_symnum];
    const char* ref_name = ref_obj.o_strtab + ref_sym.st_name;
    const bool skip_ref_obj = (flags & SYMDEF_FLAG_SKIP_REF_OBJ) != 0;

    // If this symbol is local, we can use it as-is
    if (ELF_ST_BIND(ref_sym.st_info) == STB_LOCAL) {
        def_obj = &ref_obj;
        def_sym = &ref_sym;
        return true;
    }

    // Not local, need to look it up
    uint32_t hash = CalculateHash(ref_name);
    def_obj = nullptr;
    def_sym = nullptr;
    auto lookupIt = ref_obj.o_lookup_scope.begin();
    for (/* nothing */; lookupIt != ref_obj.o_lookup_scope.end(); lookupIt++) {
        auto& entry = *lookupIt;
        auto& obj = *entry.ol_object;
        if (obj.o_sysv_bucket == nullptr)
            continue;
        if (skip_ref_obj && &obj == &ref_obj)
            continue;

        unsigned int symnum = obj.o_sysv_bucket[hash % obj.o_sysv_nbucket];
        while (symnum != STN_UNDEF) {
            if (symnum >= obj.o_sysv_nchain)
                break;

            Elf_Sym& sym = obj.o_symtab[symnum];
            const char* sym_name = obj.o_strtab + sym.st_name;
            if (sym_matches(obj, sym, ref_name)) {
                def_obj = &obj;
                def_sym = &sym;
                dbg("find_symdef(): '%s' defined in %s @ %p\n", sym_name, obj.o_name, sym.st_value);
                if (ELF_ST_BIND(def_sym->st_info) != STB_WEAK)
                    return true;
            }

            symnum = obj.o_sysv_chain[symnum];
        }
    }

    return def_sym != nullptr;
}

extern "C" Elf_Addr rtld_bind(Object* obj, size_t offs)
{
    const Elf_Rela& rela = obj->o_jmp_rela[offs];

    dbg("%s: rtld_bind(): rela: %p, offset %p sym %d type %d\n", obj->o_name, &rela, rela.r_offset,
        ELF_R_SYM(rela.r_info), ELF_R_TYPE(rela.r_info));

    Object* ref_obj;
    Elf_Sym* ref_sym;
    uint32_t symnum = ELF_R_SYM(rela.r_info);
    if (!find_symdef(*obj, symnum, false, ref_obj, ref_sym))
        die("%s: symbol '%s' not found", obj->o_name, sym_getname(*obj, symnum));
    Elf_Addr target = ref_obj->o_reloc_base + ref_sym->st_value;

    dbg("%s: sym '%s' found in %s (%p)\n", obj->o_name, sym_getname(*obj, symnum), ref_obj->o_name,
        target);

    /* Alter the jump slot so we do not have to go via the RTLD anymore */
    Elf_Addr* jmp_slot = reinterpret_cast<Elf_Addr*>(obj->o_reloc_base + rela.r_offset);
    *jmp_slot = target;
    return target;
}

int open_library(const char* name, char* path)
{
    int fd;

    // XXX for now we hardncode the path and don't care about security
    fd = OpenFile(name, ld_library_path, path);
    if (fd >= 0)
        return fd;

    return OpenFile(name, ld_default_path, path);
}

static bool elf_verify_header(const Elf_Ehdr& ehdr)
{
    // Perform basic ELF checks: header magic and version
    if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr.e_ident[EI_MAG2] != ELFMAG2 || ehdr.e_ident[EI_MAG3] != ELFMAG3)
        return false;
    if (ehdr.e_ident[EI_VERSION] != EV_CURRENT)
        return false;

    // XXX This specifically checks for amd64 at the moment
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

template<typename T>
T round_down_page(T value) { return value & ~(PAGE_SIZE - 1); }

template<typename T>
T round_up_page(T value)
{
    if ((value & (PAGE_SIZE - 1)) == 0)
        return value;

    return (value | (PAGE_SIZE - 1)) + 1;
}

static inline int elf_prot_from_phdr(const Elf_Phdr& phdr)
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

static inline int elf_flags_from_phdr(const Elf_Phdr& phdr)
{
    int flags = MAP_PRIVATE;
    return flags;
}

Object* map_object(int fd, const char* name)
{
    // Map the first page into memory - this makes things a lot easier to process
    void* first = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (first == (void*)-1)
        die("%s: mmap of header failed", name);

    auto& ehdr = *static_cast<const Elf_Ehdr*>(first);
    if (!elf_verify_header(ehdr))
        die("%s: not a (valid) ELF file", name);

    // Determine the high and low addresses of the object
    uintptr_t v_start = (uintptr_t)-1;
    uintptr_t v_end = 0;
    {
        auto phdr = reinterpret_cast<const Elf_Phdr*>(static_cast<char*>(first) + ehdr.e_phoff);
        for (unsigned int n = 0; n < ehdr.e_phnum; n++, phdr++) {
            switch (phdr->p_type) {
                case PT_LOAD:
                    uintptr_t last_addr = phdr->p_vaddr + phdr->p_memsz;
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
    auto base = reinterpret_cast<uintptr_t>(mmap(
        reinterpret_cast<void*>(v_start), v_end - v_start, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE,
        -1, 0));
    if (base == (uintptr_t)-1)
        die("%s: unable to allocate range %p-%p", name, v_start, v_end);
    sum("%s: loaded at %p-%p\n", name, base, base + (v_end - v_start));
    dbg("%s: reserved %p-%p\n", name, base, base + (v_end - v_start));

    // Now walk through the program headers and actually map them
    {
        auto phdr = reinterpret_cast<const Elf_Phdr*>(static_cast<char*>(first) + ehdr.e_phoff);
        for (unsigned int n = 0; n < ehdr.e_phnum; n++, phdr++) {
            if (phdr->p_type != PT_LOAD)
                continue;

            const auto offset = round_down_page(phdr->p_offset);
            const auto prot = elf_prot_from_phdr(*phdr);
            const auto flags = elf_flags_from_phdr(*phdr) | MAP_FIXED;

            const auto v_file_base = base + round_down_page(phdr->p_vaddr);
            const auto v_file_len = [&] {
                auto file_sz = phdr->p_filesz;
                file_sz += phdr->p_vaddr - round_down_page(phdr->p_vaddr);
                return round_up_page(file_sz);
            }();

            sum("%s: remapping %p-%p from offset %d v_file_len %d\n", name, v_file_base,
                (v_file_base + v_file_len) - 1, static_cast<int>(offset), v_file_len);
            auto p = reinterpret_cast<uintptr_t>(
                mmap(reinterpret_cast<void*>(v_file_base), v_file_len, prot, flags, fd, offset));
            if (p == (uintptr_t)-1)
                die("%s: unable to map %p-%p", name, v_file_base, v_file_base + v_end);

            if (phdr->p_filesz >= phdr->p_memsz)
                continue;

            // Handle last few bytes beyond p_filesz so they'll contain zero
            {
                const auto final_file_byte = base + phdr->p_vaddr + phdr->p_filesz;
                const auto final_file_page = round_down_page(final_file_byte);
                const auto final_file_page_end = round_up_page(final_file_byte);
                if (final_file_byte != final_file_page_end) {
                    sum("%s: clearing part of final page %p..%p\n", name, final_file_byte, final_file_page_end);
                    const bool must_make_writable = (prot & PROT_WRITE) == 0;
                    if (must_make_writable && mprotect(reinterpret_cast<void*>(final_file_page), PAGE_SIZE, prot | PROT_WRITE) < 0)
                        die("%s: cannot make final data page writable", name);

                    memset(reinterpret_cast<void*>(final_file_byte), 0, final_file_page_end - final_file_byte);
                    if (must_make_writable && mprotect(reinterpret_cast<void*>(final_file_page), PAGE_SIZE, prot) < 0)
                        die("%s: cannot make final data page readonly", name);
                }
            }

            // Take care of the mapping beyond the file
            const auto v_extra_base = base + round_up_page(phdr->p_vaddr + phdr->p_filesz);
            const auto v_extra_len = round_up_page(phdr->p_memsz - phdr->p_filesz);
            sum("%s: mapping zero data to %p..%p flags %x prot %x\n", name, v_extra_base,
                v_extra_base + v_extra_len, flags, prot);
            if (v_extra_len == 0)
                continue;
            p = reinterpret_cast<uintptr_t>(
                mmap(reinterpret_cast<void*>(v_extra_base), v_extra_len, prot, flags, -1, 0));
            if (p == (uintptr_t)-1)
                die("%s: unable to map %p-%p", name, v_extra_base, v_extra_base + v_extra_len);

            // Zero out everything beyond what is file-mapped
            // XXX the kernel should do this. XXX check if it does?
            memset(reinterpret_cast<void*>(p), 0, v_extra_len);
        }
    }

    munmap(first, PAGE_SIZE);

    struct stat sb;
    if (fstat(fd, &sb) < 0)
        die("%s: cannot fstat() %d", name, fd); // XXX shouldn't happen

    Object* obj = AllocateObject(name);
    obj->o_dev = sb.st_dev;
    obj->o_inum = sb.st_ino;
    obj->o_reloc_base = base;
    obj->o_phdr = reinterpret_cast<const Elf_Phdr*>(static_cast<char*>(first) + ehdr.e_phoff);
    obj->o_phdr_num = ehdr.e_phnum;

    process_phdr(*obj);
    if (obj->o_sysv_nbucket == 0 || obj->o_sysv_nchain == 0)
        die("%s: hash not present or unusable", name);
    setup_got(*obj);
    linkmap_add(*obj);
    return obj;
}

void dump_libs()
{
    for (const auto& object : s_Objects) {
        if (object.o_main)
            continue;
        printf("%s => 0x%p\n", object.o_name, object.o_reloc_base);
    }
}

void dump_init_fini()
{
    printf("Init funcs:\n");
    for (const auto& ole : s_InitList) {
        Object& o = *ole.ol_object;
        printf("%s (%p)\n", o.o_name, o.o_init);
    }

    printf("Fini funcs:\n");
    for (const auto& ole : s_FiniList) {
        Object& o = *ole.ol_object;
        printf("%s (%p)\n", o.o_name, o.o_fini);
    }
}

void dump_lookup_scope(Object& obj)
{
    printf("%s (%p)\n", obj.o_name, obj.o_reloc_base);
    for (const auto& ole : obj.o_lookup_scope) {
        Object& o = *ole.ol_object;
        printf("  %s (%p)\n", o.o_name, o.o_reloc_base);
    }
}

void process_init_fini_funcs(Object& obj)
{
    // First, dive into all NEEDED objects
    for (auto& needed : obj.o_needed) {
        process_init_fini_funcs(*needed.n_object);
    }

    // And add the current object, if it has an init function
    if (obj.o_init)
        ObjectListAppend(s_InitList, obj);
    if (obj.o_fini)
        ObjectListPrepend(s_FiniList, obj);
}

void run_init_funcs()
{
    for (const auto& ole : s_InitList) {
        Object& o = *ole.ol_object;
        typedef void (*FuncPtr)();
        ((FuncPtr)(o.o_init))();

        auto init_array = reinterpret_cast<Elf_Addr*>(o.o_init_array);
        if (init_array) {
            for (int n = 0; n < o.o_init_array_size; ++n) {
                using InitFn = void (*)(int, char**, char**);
                auto initFn = reinterpret_cast<InitFn>(init_array[n]);
                dbg("%s: calling initfn %d: %p\n", o.o_name, n, initFn);
                initFn(main_argc, main_argv, main_envp);
            }
        }
    }
}

void run_fini_funcs()
{
    for (const auto& ole : s_FiniList) {
        Object& o = *ole.ol_object;

        auto fini_array = reinterpret_cast<Elf_Addr*>(o.o_fini_array);
        if (fini_array) {
            for (int n = 0; n < o.o_fini_array_size; ++n) {
                using FiniFn = void (*)();
                auto finiFn = reinterpret_cast<FiniFn>(fini_array[n]);
                dbg("%s: calling finifn %d: %p\n", o.o_name, n, finiFn);
                finiFn();
            }
        }

        typedef void (*FuncPtr)();
        ((FuncPtr)(o.o_fini))();
    }
}

extern "C" __attribute__((visibility("default"))) int
dl_iterate_phdr(int (*callback)(struct dl_phdr_info* info, size_t size, void* data), void* data)
{
    for (const auto& obj : s_Objects) {
        struct dl_phdr_info info;
        memset(&info, 0, sizeof(info));
        info.dlpi_addr = obj.o_reloc_base;
        info.dlpi_name = obj.o_name;
        info.dlpi_phdr = obj.o_phdr;
        info.dlpi_phnum = obj.o_phdr_num;
        int result = callback(&info, sizeof(info), data);
        if (result != 0)
            return result;
    }

    return 0;
}

extern "C" Elf_Addr rtld(register_t* stk, uintptr_t* exit_func)
{
    uint64_t ei_interpreter_base = 0;
    uint64_t ei_phdr = 0;
    uint64_t ei_phdr_entries = 0;
    uint64_t ei_entry = 0;
    auto stk_orig = stk;
    {
        dbg("init stk %p\n", stk);
        main_argc = *stk++;
        main_argv = reinterpret_cast<char**>(stk);
        for (auto argc = main_argc; argc > 0; --argc)
            stk++; // skip args
        stk++;     // skip null
        main_envp = reinterpret_cast<char**>(stk);
        while (*stk != 0)
            stk++; // skip envp
        stk++;     // skip null
        dbg("stk %p\n", stk);
        for (auto auxv = reinterpret_cast<const Elf_Auxv*>(stk); auxv->a_type != AT_NULL; auxv++) {
            dbg("auxv tag %d\n", auxv->a_type);
            switch (auxv->a_type) {
                case AT_BASE:
                    ei_interpreter_base = reinterpret_cast<uintptr_t>(auxv->a_un.a_ptr);
                    break;
                case AT_PHDR:
                    ei_phdr = reinterpret_cast<uintptr_t>(auxv->a_un.a_ptr);
                    break;
                case AT_PHENT:
                    ei_phdr_entries = auxv->a_un.a_val;
                    break;
                case AT_ENTRY:
                    ei_entry = reinterpret_cast<uintptr_t>(auxv->a_un.a_ptr);
                    break;
                default:
                    dbg("unrecognized auxv type %d val %p\n", auxv->a_type, auxv->a_un.a_val);
            }
        }
    }

    /*
     * We are a dynamic executable, which means the kernel just chose a
     * virtual address to place us. We have to adjust our dynamic
     * relocations to match - we have been loaded at ei_interpreter_base.
     */
    assert(ei_interpreter_base != 0);
    assert(ei_entry != 0);
    assert(ei_phdr != 0);
    assert(ei_phdr_entries != 0);
    rtld_relocate(ei_interpreter_base);

    // Get access to the command line and environment; we need this to
    // find our program name and any LD_... settings
    InitializeFromStack(stk_orig);
    debug |= IsEnvironmentVariableSet("LD_DEBUG");

    // Now, locate our executable and process it
    auto main_obj = AllocateObject(GetProgName());
    main_obj->o_main = true;
    main_obj->o_phdr = reinterpret_cast<const Elf_Phdr*>(ei_phdr);
    main_obj->o_phdr_num = ei_phdr_entries;
    process_phdr(*main_obj);
    setup_got(*main_obj);

    // Hook the main program and the LDSO to the linker map */
    linkmap_add(*main_obj);
    linkmap_add(*s_Objects.begin());

    // Handle LD_LIBRARY_PATH - we'll be loading extra things from here on
    ld_library_path = getenv("LD_LIBRARY_PATH");

    /*
     * Walk through all objects and process their needed parts. We use the
     * fact that things are added in-order to s_Objects here.
     *
     * Note that s_Objects will change while we are walking through this list!
     */
    for (auto& obj : s_Objects) {
        for (auto& needed : obj.o_needed) {
            const char* name = obj.o_strtab + needed.n_name_idx;
            char path[PATH_MAX];
            int fd = open_library(name, path);
            if (fd < 0)
                die("unable to open library '%s', aborting\n", name);

            struct stat sb;
            if (fstat(fd, &sb) < 0)
                die("unable to stat library '%s', aborting\n", name); // XXX shouldn't happen

            // Skip libraries we already have
            Object* found_obj = FindLibrary(
                [&](const Object& o) { return o.o_dev == sb.st_dev && o.o_inum == sb.st_ino; });
            if (found_obj != nullptr) {
                close(fd);
                if (found_obj == &obj)
                    die("%s: depends on itself", name);
                needed.n_object = found_obj;
                continue;
            }

            // New library; load it - this also adds it to s_Objects, which we
            // will pick up in a future iteration
            Object* new_obj = map_object(fd, path);
            if (new_obj == nullptr)
                die("cannot load library '%s', aborting\n", name);
            needed.n_object = new_obj;
            close(fd);
        }
    }

    auto& rtld_obj = *s_Objects.begin();
    for (auto& obj : s_Objects) {
        ObjectInitializeLookupScope(obj, obj, *main_obj);

        // At the very end, insert the dynamic loader we can provide dl_iterate_phdr
        // and dl...()
        ObjectListAppend(obj.o_lookup_scope, rtld_obj);

        if (!debug)
            continue;
        printf("lookup scope for '%s'\n", obj.o_name);
        dump_lookup_scope(obj);
    }

    // Process all relocations
    for (auto& obj : s_Objects) {
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
    dbg("transferring control to %p\n", ei_entry);
    *exit_func = reinterpret_cast<uintptr_t>(&run_fini_funcs);
    return ei_entry;
}
