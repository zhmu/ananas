#ifndef __ELF_H__
#define __ELF_H__

/* Basic datatypes */
typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef  int32_t Elf32_Sword;
typedef uint32_t Elf32_Word;

#define EI_NIDENT	16

typedef struct {
	unsigned char	e_ident[EI_NIDENT];
#define EI_MAG0		0	/* File identification */
#define EI_MAG1		1	/* File identification */
#define EI_MAG2		2	/* File identification */
#define EI_MAG3		3	/* File identification */
#define  ELFMAG0	0x7f
#define  ELFMAG1	'E'
#define  ELFMAG2	'L'
#define  ELFMAG3	'F'
#define EI_CLASS	4	/* File class */
#define  ELFCLASSNONE	0	/* Invalid class */
#define  ELFCLASS32	1	/* 32-bit objects */
#define  ELFCLASS64	2	/* 64-bit objects */
#define EI_DATA		5	/* Data encoding */
#define  ELFDATANONE	0	/* Invalid data encoding */
#define  ELFDATA2LSB	1	/* Least Significant (0x0102 stored 0x02 0x01) */
#define  ELFDATA2MSB	2	/* Most Significant (0x0102 stored 0x01 0x02) */
#define EI_VERSION	6	/* File verison */
#define EI_PAD		7	/* Start of padding bytes */
	Elf32_Half	e_type;
#define ET_NONE		0	/* No file type */
#define ET_REL		1	/* Relocatable file */
#define ET_EXEC		2	/* Executable file */
#define ET_DYN		3	/* Shared object file */
#define ET_CORE		4	/* Core file */
#define ET_LOPROC	0xff00	/* Reserved */
#define ET_HIPROC	0xffff	/* Reserved */
	Elf32_Half	e_machine;
#define EM_M32		1	/* AT&T WE 32100 */
#define EM_SPARC	 2	/* SPARC */
#define EM_386		3	/* Intel Architecture */
#define EM_68K		4	/* Motorola 68000 */
#define EM_88K		5	/* Motorola 88000 */
#define EM_860		7	/* Intel 80860 */
#define EM_MIPS		8	/* MIPS RS3000 Big-Endian */
#define EM_MIPS_RS4_BE	9	/* MIPS RS4000 Big-Endian */
	Elf32_Word	e_version;
#define EV_NONE		0	/* Invalid version */
#define EV_CURRENT	1	/* Current version */
	Elf32_Addr	e_entry;
	Elf32_Off	e_phoff;
	Elf32_Off	e_shoff;
	Elf32_Word	e_flags;
	Elf32_Half	eh_size;
	Elf32_Half	e_phentsize;
	Elf32_Half	e_phnum;
	Elf32_Half	e_shentsize;
	Elf32_Half	e_shnum;
#define SHN_UNDEF	0
#define SHN_LORESERVE	0xff00
#define SHN_LOPROC	0xff00
#define SHN_HIPROC	0xff1f
#define SHN_ABS		0xfff1
#define SHN_COMMON	0xfff2
#define SH_HIRESERVE	0xffff
	Elf32_Half	e_shstrndx;
} Elf32_Ehdr;

typedef struct {
	Elf32_Word	sh_name;
#define SHT_NULL	0			/* Inactive */
#define SHT_PROGBITS	1			/* Program with data */
#define SHT_SYMTAB	2			/* Symbol table */
#define SHT_STRTAB	3			/* String table */
#define SHT_RELA	4			/* Relocation entries with addends */
#define SHT_HASH	5			/* Symbol hash table */
#define SHT_DYNAMIC	6			/* Dynamic linking */
#define SHT_NOTE	7			/* File marker notes */
#define SHT_NOBITS	8			/* Program without data */
#define SHT_REL		9			/* Relocation entries without addeneds */
#define SHT_SHLIB	10			/* ... */
#define SHT_DYNSYM	11			/* Symbol table */
#define SHT_LOPROC	0x70000000
#define SHT_HIPROC	0x7fffffff
#define SHT_LOUSER	0x80000000
#define SHT_HIUSER	0xffffffff
	Elf32_Word	sh_type;
#define SHF_WRITE	0x01			/* Writable data */
#define SHF_ALLOC	0x02			/* Must be allocated */
#define SHF_EXECINSTR	0x04			/* Executable instructions */
#define SHF_MASKPROC	0xf0000000
	Elf32_Word	sh_flags;
	Elf32_Addr	sh_addr;
	Elf32_Off	sh_offset;
	Elf32_Word	sh_size;
	Elf32_Word	sh_link;
	Elf32_Word	sh_info;
	Elf32_Word	sh_addralign;
	Elf32_Word	sh_entsize;
}  Elf32_Shdr;

typedef struct {
	Elf32_Word	st_name;		/* String table index */
	Elf32_Addr	st_value;		/* Symbol value */
	Elf32_Word	st_size;		/* Symbol size */
	unsigned char	st_info;		/* Type/binding attributes */
#define ELF32_ST_BIND(i)   ((i)>>4)
#define STB_LOCAL	0			/* Local symbol */
#define STB_GLOBAL	1			/* Global symbol */
#define STB_WEAK	2			/* Weak symbol */
#define STB_LOPROC	13
#define STB_HIPROC	15
#define ELF32_ST_TYPE(i)   ((i)&0xf)
#define STT_NOTYPE	0			/* Unspecified type */
#define STT_OBJECT	1			/* Data object symbol */
#define STT_FUNC	2			/* Function symbol */
#define STT_SECTION	3			/* Section symbol */
#define STT_FILE	4			/* File symbol */
#define STT_LOPROC	13
#define STT_HIPROC	15
#define ELF32_ST_INFO(b,t) (((b)<<4)+((t)&0xf))
	unsigned char	st_other;		/* 0 */
	Elf32_Half	st_shndx;		/* Section header table */
} Elf32_Sym;

typedef struct {
	Elf32_Addr	r_offset;		/* Relocation action offset */
	Elf32_Word	r_info;			/* Relocation info */
#define ELF32_R_SYM(i)		((i)>>8)
#define ELF32_R_TYPE(i)		((unsigned char)i)
#define ELF32_R_INFO(s,t)	(((s)<<8)+(unsigned char)(t))
} Elf32_Rel;

typedef struct {
	Elf32_Addr	r_offset;		/* Relocation action offset */
	Elf32_Word	r_info;			/* Relocation info */
	Elf32_Sword	r_addend;		/* Constant addend */
} Elf32_Rela;

typedef struct {
	Elf32_Word	p_type;			/* Segment type */
#define PT_NULL		0			/* Unused */
#define PT_LOAD		1			/* Loadable segment */
#define PT_DYNAMIC	2			/* Dynamic linking information */
#define PT_INTERP	3			/* Interpreter to use */
#define PT_NOTE		4			/* Auxiliary information */
#define PT_SHLIB	5			/* Reserved */
#define PT_PHDR		6			/* Program header */
#define PT_LOPROC	0x70000000
#define PT_HIPROC	0x7fffffff
	Elf32_Off	p_offset;		/* Segment offset in file */
	Elf32_Addr	p_vaddr;		/* Virtual address */
	Elf32_Addr	p_paddr;		/* Physical address */
	Elf32_Word	p_filesz;		/* Number of bytes in image */
	Elf32_Word	p_memsz;		/* Number of bytes in memory */
	Elf32_Word	p_flags;		/* Flags */
	Elf32_Word	p_align;		/* Alignment restriction */
} Elf32_Phdr;

#endif /* __ELF_H__ */
