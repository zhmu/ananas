#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CHUNK_SIZE 1024
char chunk[CHUNK_SIZE];

void
usage()
{
	fprintf(stderr, "usage: txt2inc [-h?] input.txt output.inc identifier\n\n");
	fprintf(stderr, "  -h, -?     this help\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char* argv[])
{
	int ch;
	FILE* in;
	FILE* out;
	size_t len;

	while ((ch = getopt(argc, argv, "h?")) != -1) {
		switch(ch) {
			case 'h':
			case '?':
				usage();
				/* NOTREACHED */
		}
	}
	argc -= optind; argv += optind;
	if (argc != 3) {
		fprintf(stderr, "error: %u arguments needed, %u given\n", 3, argc);
		usage();
		/* NOTREACHED */
	}

	in = fopen(argv[0], "r");
	if (in == NULL)
		err(1, "unable to open '%s'", argv[0]);

	fseek(in, 0, SEEK_END);
	len = ftell(in);
	rewind(in);

	out = fopen(argv[1], "w");
	if (out == NULL)
		err(1, "unable to open '%s'", argv[1]);

	fprintf(out, "/* Automatically generated - do not edit */\n\n");
	fprintf(out, "#define %s_LENGTH %lu\n", argv[2], len);
	fprintf(out, "unsigned char %s[%lu] = {\n", argv[2], len);

	size_t left = len;
	int col_count = 0;
	while (left > 0) {
		size_t chunklen = left > CHUNK_SIZE ? CHUNK_SIZE : left;

		if (!fread(chunk, chunklen, 1, in))
			err(1, "read error");

		size_t i;
		for (i = 0; i < chunklen; i++) {
			if (col_count == 0)
				fprintf(out, "\t");

			fprintf(out, "0x%02x, ", (unsigned char)chunk[i]);

			col_count++;
			if (col_count > 8) {
				fprintf(out, "\n");
				col_count = 0;
			}
		}
		left -= chunklen;
	}
	if (col_count > 0)
		fprintf(out, "\n");
	fprintf(out, "};\n");
	fclose(out);
	fclose(in);

	return 0;
}
