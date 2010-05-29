#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTOR_SIZE 512

void
load_file(const char* name, char** buffer, unsigned long* len)
{
	FILE* f = fopen(name, "rb");
	if (f == NULL)
		err(1, "cannot open %s", name);
	fseek(f, 0, SEEK_END); *len = ftell(f); rewind(f);
	*buffer = (char*)malloc(*len);
	if (*buffer == NULL)
		errx(1, "out of memory");
	if (!fread(*buffer, *len, 1, f))
		err(1, "cannot load %s", name);
	fclose(f);
}

/*
 * This is a quick 'n dirty tool that injects 'bootblock' and 'bootcode'
 * into an image.
 */
int
main(int argc, char* argv[])
{
	if (argc != 4) {
		printf("usage: merge_mbr bootblock bootcode image.bin\n");
		return 1;
	}

	char* bootblock; unsigned long bootblock_len;
	load_file(argv[1], &bootblock, &bootblock_len);
	if (bootblock_len >= 0x1b4)
		errx(1, "%s is too large, rejected", argv[1]);

	char* bootcode; unsigned long bootcode_len;
	load_file(argv[2], &bootcode, &bootcode_len);

	FILE* f = fopen(argv[3], "r+b");
	if (f == NULL)
		err(1, "cannot open %s", argv[3]);
	unsigned char bootsector[SECTOR_SIZE];
	rewind(f);
	if (!fread(bootsector, SECTOR_SIZE, 1, f))
		err(1, "cannot read %s", argv[3]);
	if (bootsector[SECTOR_SIZE - 2] != 0x55 ||
	    bootsector[SECTOR_SIZE - 1] != 0xaa)
		errx(1, "rejecting image, no mbr signature found");

	memcpy(bootsector, bootblock, bootblock_len);

	rewind(f);
	if (!fwrite(bootsector, SECTOR_SIZE, 1, f))
		err(1, "cannot write %s", argv[3]);

	if (!fwrite(bootcode, bootcode_len, 1, f))
		err(1, "cannot write %s", argv[3]);

	fclose(f);

	return 0;
}

/* vim:set ts=2 sw=2: */
