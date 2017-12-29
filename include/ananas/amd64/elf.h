#ifndef ANANAS_AMD64_ELF_H
#define ANANAS_AMD64_ELF_H

#include <ananas/elf.h>

typedef Elf64_Ehdr Elf_Ehdr;
typedef Elf64_Phdr Elf_Phdr;
typedef Elf64_Dyn Elf_Dyn;
typedef Elf64_Sym Elf_Sym;
typedef Elf64_Rela Elf_Rela;
typedef Elf64_Addr Elf_Addr;
typedef Elf64_Half Elf_Half;
typedef uint64_t Elf_BloomWord;

#define ELF_R_SYM ELF64_R_SYM
#define ELF_R_TYPE ELF64_R_TYPE
#define ELF_ST_BIND ELF64_ST_BIND
#define ELF_ST_TYPE ELF64_ST_TYPE

#endif /* ANANAS_AMD64_ELF_H */
