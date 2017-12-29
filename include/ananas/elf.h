#ifndef __ELF_H__
#define __ELF_H__

/* Basic datatypes */
typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef  int32_t Elf32_Sword;
typedef uint32_t Elf32_Word;

typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef  int32_t Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef  int64_t Elf64_Sxword;

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
#define EI_VERSION	6	/* File version */
#define EI_OSABI	7	/* OS/ABI identification */
 #define ELFOSABI_SYSV	0	/* System V ABI */
 #define ELFOSABI_HPUX	1	/* HP-UX */
 #define ELFOSABI_STANDALONE 255 /* Standalone (embedded) application */
#define EI_ABIVERSION	8	/* ABI version */
#define EI_PAD		9	/* Start of padding bytes */
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
#define EM_X86_64	62	/* AMD64 */
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
	unsigned char	e_ident[EI_NIDENT];	/* ELF identification */
	Elf64_Half	e_type;			/* Object file type */
	Elf64_Half	e_machine;		/* Machine type */
	Elf64_Word	e_version;		/* Object file version */
	Elf64_Addr	e_entry;		/* Entry point address */
	Elf64_Off	e_phoff;		/* Program header offset */
	Elf64_Off	e_shoff;		/* Section header offset */
	Elf64_Word	e_flags;		/* Processor-specific flags */
	Elf64_Half	e_ehsize;		/* ELF header size */
	Elf64_Half	e_phentsize;		/* Size of program header entry */
	Elf64_Half	e_phnum;		/* Number of program header entries */
	Elf64_Half	e_shentsize;		/* Size of section header entry */
	Elf64_Half	e_shnum;		/* Number of seciton header entries */
	Elf64_Half	e_shstrndx;		/* Section name string table index */
} Elf64_Ehdr;

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
	Elf64_Word	sh_name;		/* Section name */
	Elf64_Word	sh_type;		/* Section type */
	Elf64_Xword	sh_flags;		/* Section attributes */
	Elf64_Addr	sh_addr;		/* Virtual address in memory */
	Elf64_Off	sh_offset;		/* Offset in file */
	Elf64_Xword	sh_size;		/* Size of section */
	Elf64_Word	sh_link;		/* Link to other section */
	Elf64_Word	sh_info;		/* Miscellaneous information */
	Elf64_Xword	sh_addralign;		/* Address alignment boundary */
	Elf64_Xword	sh_entsize;		/* Size of entries, if section ha table */
} Elf64_Shdr;

#define STN_UNDEF	0

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
	Elf64_Word	st_name;		/* Symbol name */
	unsigned char	st_info;		/* Type and Binding attributes */
#define ELF64_ST_BIND(i)   ((i)>>4)
#define ELF64_ST_TYPE(i)   ((i)&0xf)
	unsigned char	st_other;		/* Reserved */
	Elf64_Half	st_shndx;		/* Section table index */
	Elf64_Addr	st_value;		/* Symbol value */
	Elf64_Xword	st_size;		/* Size of object (e.g., common) */
} Elf64_Sym;

typedef struct {
	Elf32_Addr	r_offset;		/* Relocation action offset */
	Elf32_Word	r_info;			/* Relocation info */
#define ELF32_R_SYM(i)		((i)>>8)
#define ELF32_R_TYPE(i)		((unsigned char)i)
#define ELF32_R_INFO(s,t)	(((s)<<8)+(unsigned char)(t))
#define R_386_NONE	0			/* none */
#define R_386_32	1			/* S + A */
#define R_386_PC32	2			/* S + A - P */
#define R_386_GOT32	3			/* G + A */
#define R_386_PL32	4			/* L + T - P */
#define R_386_COPY	5			/* none */
#define R_386_GLOB_DAT	6			/* S */
#define R_386_JMP_SLOT	7			/* S */
#define R_386_RELATIVE	8			/* B + A */
#define R_386_GOTOFF	9			/* S + A - GOT */
#define R_386_GOTPC	10			/* GOT + A - P */
} Elf32_Rel;

typedef struct {
	Elf64_Addr	r_offset;		/* Address of reference */
	Elf64_Xword	r_info;			/* Symbol index and type of relocation */
#define ELF64_R_SYM(i)		((i)>>32)
#define ELF64_R_TYPE(i)		((i)&0xffffffff)
#define ELF64_R_INFO(s,t)	(((s)<<32)+((t)&0xffffffff))
#define R_X86_64_NONE			0	/* none */
#define R_X86_64_64			1	/* (word64) S + A */
#define R_X86_64_PC32			2	/* (word32) S + A - P */
#define R_X86_64_GOT32			3	/* (word32) G + A */
#define R_X86_64_PLT32			4	/* (word32) L + A - P */
#define R_X86_64_COPY			5	/* none */
#define R_X86_64_GLOB_DAT		6	/* (word64) S */
#define R_X86_64_JUMP_SLOT		7	/* (word64) S */
#define R_X86_64_RELATIVE		8	/* (word64) B + A */
#define R_X86_64_GOTPCREL		9	/* (word32) G + GOT + A - P */
#define R_X86_64_32			10	/* (word32) S + A */
#define R_X86_64_32S			11	/* (word32) S + A */
#define R_X86_64_16			12	/* (word16) S + A */
#define R_X86_64_PC16			13	/* (word16) S + A - P */
#define R_X86_64_8			14	/* (word8) S + A */
#define R_X86_64_PC8			15	/* (word8) S + A - P */
#define R_X86_64_DTPMOD64		16	/* (word64)*/
#define R_X86_64_DTPOFF64		17	/* (word64) */
#define R_X86_64_TPOFF64		18	/* (word64) */
#define R_X86_64_TLSGD			19	/* (word32) */
#define R_X86_64_TLSLD			20	/* (word32) */
#define R_X86_64_DTPOFF32		21	/* (word32) */
#define R_X86_64_GOTTPOFF		22	/* (word32) */
#define R_X86_64_TPOFF32		23	/* (word32) */
#define R_X86_64_PC64			24	/* (word64) S + A - P */
#define R_X86_64_GOTOFF64		25	/* (word64) S + A - GOT */
#define R_X86_64_GOTPC32		26	/* (word32) GOT + A - P */
#define R_X86_64_SIZE32			32	/* (word32) Z + A */
#define R_X86_64_SIZE64			33	/* (word64) Z + A */
#define R_X86_64_GOTPC32_TLSDESC	34	/* (word32) */
#define R_X86_64_TLSDESC_CALL		35	/* none */
#define R_X86_64_TLSDESC		36	/* (word64 x 2) */
} Elf64_Rel;

typedef struct {
	Elf32_Addr	r_offset;		/* Relocation action offset */
	Elf32_Word	r_info;			/* Relocation info */
	Elf32_Sword	r_addend;		/* Constant addend */
} Elf32_Rela;

typedef struct {
	Elf64_Addr	r_offset;		/* Address of reference */
	Elf64_Xword	r_info;			/* Symbol index and type of relocation */
	Elf64_Sxword	r_addend;		/* Constant part of expression */
} Elf64_Rela;

typedef struct {
	Elf32_Word	p_type;			/* Segment type */
#define PT_NULL		0			/* Unused */
#define PT_LOAD		1			/* Loadable segment */
#define PT_DYNAMIC	2			/* Dynamic linking information */
#define PT_INTERP	3			/* Interpreter to use */
#define PT_NOTE		4			/* Auxiliary information */
#define PT_SHLIB	5			/* Reserved */
#define PT_PHDR		6			/* Program header */
#define PT_GNU_EH_FRAME	0x6474e550
#define PT_GNU_STACK	0x6474e551
#define PT_GNU_RELRO	0x6474e552
#define PT_LOPROC	0x70000000
#define PT_HIPROC	0x7fffffff
	Elf32_Off	p_offset;		/* Segment offset in file */
	Elf32_Addr	p_vaddr;		/* Virtual address */
	Elf32_Addr	p_paddr;		/* Physical address */
	Elf32_Word	p_filesz;		/* Number of bytes in image */
	Elf32_Word	p_memsz;		/* Number of bytes in memory */
	Elf32_Word	p_flags;		/* Flags */
#define PF_X		0x1			/* Execute */
#define PF_W		0x2			/* Write */
#define PF_R		0x4			/* Read */
	Elf32_Word	p_align;		/* Alignment restriction */
} Elf32_Phdr;

typedef struct {
	Elf64_Word	p_type;			/* Type of segment */
	Elf64_Word	p_flags;		/* Segment attributes */
	Elf64_Off	p_offset;		/* Offset in file */
	Elf64_Addr	p_vaddr;		/* Virtual address in memory */
	Elf64_Addr	p_paddr;		/* Reserved */
	Elf64_Xword	p_filesz;		/* Size of segment in file */
	Elf64_Xword	p_memsz;		/* Size of segment in memory */
	Elf64_Xword	p_align;		/* Alignment of segment */
} Elf64_Phdr;

typedef struct {
	Elf32_Sword	d_tag;
	union {
		Elf32_Word	d_val;
		Elf32_Addr	d_ptr;
	} d_un;
} Elf32_Dyn;

typedef struct {
	Elf64_Sxword	d_tag;
#define DT_NULL		0	/* Marks end of the dynamic array */
#define DT_NEEDED	1	/* The string table offset of the name of a needed library */
#define DT_PLTRELSZ	2	/* Total size, in bytes, of the relocation entries associated with the procedure linkage table */
#define DT_PLTGOT	3	/* Contains an address associated with the linkage table */
#define DT_HASH		4	/* Address of the symbol hash table */
#define DT_STRTAB	5	/* Address of the dynamic string table */
#define DT_SYMTAB	6	/* Address of the dynamic symbol table */
#define DT_RELA		7	/* Address of a relocation table with Elf64_Rela entries */
#define DT_RELASZ	8	/* Total size, in bytes, of the DT_RELA relocation table */
#define DT_RELAENT	9	/* Size, in bytes, of each DT_RELA relocation entry */
#define DT_STRSZ	10	/* Total size, in bytes, of the string table */
#define DT_SYMENT	11	/* Size, in bytes, of each symbol table entry */
#define DT_INIT		12	/* Address of the initialization function */
#define DT_FINI		13	/* Address of the termination function */
#define DT_SONAME	14	/* The string table offset of the name of this shared object */
#define DT_RPATH	15	/* The string table offset of a shared library search path string */
#define DT_SYMBOLIC	16	/* Presence modifies symbol resolution algorithm */
#define DT_REL		17	/* Address of a relocation table with Elf64_Rel entries */
#define DT_RELSZ	18	/* Total size, in bytes, of the DT_REL relocation table */
#define DT_RELENT	19	/* Size, in bytes, of each DT_REL relocation entry */
#define DT_PLTREL	20	/* Type of relocation entries used for procedure linkage table (DT_REL / DT_RELA) */
#define DT_DEBUG	21	/* Reserved for debugger use */
#define DT_TEXTREL	22	/* Presence signals that the relocation table contains relocation for a non-writable segment */
#define DT_JMPREL	23	/* Address of the relocations associated with the procedure linkage table */
#define DT_BIND_NOW	24	/* Presence signals that all relocations must be processed before transferring control to program */
#define DT_INIT_ARRAY	25	/* Pointer to an array of pointers of initalization functions */
#define DT_FINI_ARRAY	26	/* Pointer to an array of pointers of termination functions */
#define DT_INIT_ARRAYSZ	27	/* Size, in bytes, of the array of initialization functions */
#define DT_FINI_ARRAYSZ	28	/* Size, in bytes, of the array of termination functions */
#define DT_LOOS		0x60000000 /* Defines a range of dynamic table tags for environment-specific use */
#define DT_GNU_HASH	0x6ffffef5
#define DT_HIOS		0x6fffffff
#define DT_LOPROC	0x70000000 /* Defines a range of dynamic table tags for processor-specific use */
#define DT_HIPROC	0x7fffffff
	union {
		Elf64_Xword	d_val;
		Elf64_Addr	d_ptr;
	} d_un;
} Elf64_Dyn;

#endif /* __ELF_H__ */
