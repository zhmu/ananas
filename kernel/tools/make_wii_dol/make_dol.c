#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../include/elf.h"

#define DOL_NUM_TEXT 7
#define DOL_NUM_DATA 11
#define DOL_SECTION_PAD 0x100

#define DOL_ADDRESS_ORMASK 0x80000000

struct DOL_HEADER {
	uint32_t	text_filepos[DOL_NUM_TEXT];
	uint32_t	data_filepos[DOL_NUM_DATA];
	uint32_t	text_memaddr[DOL_NUM_TEXT];
	uint32_t	data_memaddr[DOL_NUM_DATA];
	uint32_t	text_length[DOL_NUM_TEXT];
	uint32_t	data_length[DOL_NUM_DATA];
	uint32_t	bss_addr;
	uint32_t	bss_length;
	uint32_t	entry;
	uint8_t	  unused[0x1c];
} __attribute__((packed));

static struct DOL_HEADER dol;
static FILE* dol_file = NULL;
static uint8_t dol_pad[DOL_SECTION_PAD] = { 0 };

#include "trampoline.inc"

#define TRAMPOLINE_ADDRESS 0x80004000
#define TRAMPOLINE_ENTRY_OFFSET_HI 2
#define TRAMPOLINE_ENTRY_INITIAL_HI 0x1234
#define TRAMPOLINE_ENTRY_OFFSET_LO 6
#define TRAMPOLINE_ENTRY_INITIAL_LO 0x4321

static void
usage()
{
	fprintf(stderr, "usage: make_dol output.dol kernel.elf\n");
	exit(EXIT_FAILURE);
}

#define FROM_BE_16(x) \
	(((x) >> 8) | ((x) & 0xff) << 8)
#define FROM_BE_32(x) \
	((((x) >> 24) | (((x) >> 16) & 0xff) << 8) | \
	 ((((x) >> 8) & 0xff) << 16) | (((x) & 0xff) << 24))

#define TO_BE_16(x) \
	FROM_BE_16(x)
#define TO_BE_32(x) \
	FROM_BE_32(x)

static void
dol_add_data(uint32_t addr, void* buffer, size_t length)
{
	unsigned int i = 0;
	for (i = 0; i < DOL_NUM_TEXT; i++)
		if (dol.data_length[i] == 0)
			break;
	if (i == DOL_NUM_DATA)
		errx(1, "too many dol data sections");

	long pos = ftell(dol_file);

	/* if we are not padded, do so (is this needed?) */
	if (pos % DOL_SECTION_PAD) {
		int todo = DOL_SECTION_PAD - pos % DOL_SECTION_PAD;
		if (!fwrite(dol_pad, todo, 1, dol_file))
			err(1, "fwrite");
		pos += todo;
	}

	if (!fwrite(buffer, length, 1, dol_file))
		err(1, "fwrite");

	dol.data_filepos[i] = TO_BE_32(pos);
	dol.data_memaddr[i] = TO_BE_32(addr | DOL_ADDRESS_ORMASK);
	dol.data_length[i]  = TO_BE_32(length);
}

static void
dol_add_text(uint32_t addr, void* buffer, size_t length)
{
	unsigned int i = 0;
	for (i = 0; i < DOL_NUM_TEXT; i++)
		if (dol.text_length[i] == 0)
			break;
	if (i == DOL_NUM_TEXT)
		errx(1, "too many dol text sections");

	long pos = ftell(dol_file);
	if (pos % DOL_SECTION_PAD) {
		int todo = DOL_SECTION_PAD - pos % DOL_SECTION_PAD;
		if (!fwrite(dol_pad, todo, 1, dol_file))
			err(1, "fwrite");
		pos += todo;
	}

	if (!fwrite(buffer, length, 1, dol_file))
		err(1, "fwrite");

	dol.text_filepos[i] = TO_BE_32(pos);
	dol.text_memaddr[i] = TO_BE_32(addr | DOL_ADDRESS_ORMASK);
	dol.text_length[i]  = TO_BE_32(length);
}

static void
elf_load(const char* fname)
{
	Elf32_Ehdr ehdr;

	FILE* f = fopen(fname, "rb");
	if (f == NULL)
		err(1, "fopen(%s)", fname);
	if (!fread(&ehdr, sizeof(ehdr), 1, f))
		err(1, "fread");

	/* Perform basic ELF checks */
	if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr.e_ident[EI_MAG2] != ELFMAG2 || ehdr.e_ident[EI_MAG3] != ELFMAG3)
		errx(1, "ELF magic mismatch (not an ELF file?)");
	if (ehdr.e_ident[EI_DATA] != ELFDATA2MSB)
		errx(1, "Not a big-endian file");
	if (ehdr.e_ident[EI_CLASS] != ELFCLASS32)
		errx(1, "Not a 32 bit ELF file");
	if (ehdr.e_ident[EI_VERSION] != EV_CURRENT)
		errx(1, "Version mismatch");
	if (FROM_BE_16(ehdr.e_type) != ET_EXEC)
		errx(1, "Not an executable");
	if (FROM_BE_16(ehdr.e_phnum) == 0)
		errx(1, "No program table there");

	/* Patch the trampolineer code so that it will do a correct jump to our code */
	uint32_t entry = FROM_BE_32(ehdr.e_entry);
	*(uint16_t*)&trampoline[TRAMPOLINE_ENTRY_OFFSET_HI] = TO_BE_16(entry >> 16);
	*(uint16_t*)&trampoline[TRAMPOLINE_ENTRY_OFFSET_LO] = TO_BE_16(entry & 0xffff);

	/* Place the loader before everything else */
	dol_add_text(TRAMPOLINE_ADDRESS, trampoline, sizeof(trampoline));
	dol.entry = TO_BE_32(TRAMPOLINE_ADDRESS);

	/* Add the entry points, one by one */
	for (unsigned int i = 0; i < FROM_BE_16(ehdr.e_phnum); i++) {
		Elf32_Phdr phdr;
		fseek(f, FROM_BE_32(ehdr.e_phoff) + i * FROM_BE_16(ehdr.e_phentsize), SEEK_SET);
		if (!fread(&phdr, sizeof(phdr), 1, f))
			err(1, "fread");
		if(FROM_BE_32(phdr.p_type) != PT_LOAD)
			continue;

		if (FROM_BE_32(phdr.p_vaddr) != FROM_BE_32(phdr.p_paddr))
			errx(1, "physical != virtual address");

		/* OK, we have a piece that we may need to load */
		size_t memsz  = FROM_BE_32(phdr.p_memsz);
		size_t filesz = FROM_BE_32(phdr.p_filesz);
		if (memsz == 0) {
			/* Piece does not contain anything in-memory: ignore */
			continue;
		}
		void* data = malloc(memsz);
		if (data == NULL)
			err(1, "malloc");

		/* Ensure we properly zero-out files that are not stored on disk! */
		memset(data, 0, memsz);

		fseek(f, FROM_BE_32(phdr.p_offset), SEEK_SET);
		if (!fread(data, filesz, 1, f))
			err(1, "fread");

		dol_add_data(FROM_BE_32(phdr.p_paddr), data, memsz);
		free(data);
	}
	fclose(f);
}

static void
trampoline_verify()
{
	/* Just a sanity check to ensure the trampoline and defines are in sync */
	if (FROM_BE_16(*(uint16_t*)&trampoline[TRAMPOLINE_ENTRY_OFFSET_HI]) != TRAMPOLINE_ENTRY_INITIAL_HI)
		errx(1, "hi offset magic value does not match in trampoline");
	if (FROM_BE_16(*(uint16_t*)&trampoline[TRAMPOLINE_ENTRY_OFFSET_LO]) != TRAMPOLINE_ENTRY_INITIAL_LO)
		errx(1, "lo offset magic value does not match in trampoline");
}

int
main(int argc, char* argv[])
{
	memset(&dol, 0, sizeof(dol));

	trampoline_verify();

	if (argc != 3)
		usage();

	dol_file = fopen(argv[1], "wb");
	if (dol_file == NULL)
		err(1, "cannot create %s", argv[1]);

	/*
	 * First of all, write the dummy DOL header to the file; it'll be updated
	 * afterwards and this gives us correct offsets from the start.
	 */
	if (!fwrite(&dol, sizeof(dol), 1, dol_file))
		err(1, "fwrite");
	
	elf_load(argv[2]);

	/* Overwrite the DOL header */
	fseek(dol_file, 0, SEEK_SET);
	if (!fwrite(&dol, sizeof(dol), 1, dol_file))
		err(1, "fwrite");

	fclose(dol_file);
	return 0;
}

/* vim:set ts=2 sw=2: */
