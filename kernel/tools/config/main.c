#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define KEYWORD_UNKNOWN		0
#define KEYWORD_ARCH			1
#define KEYWORD_DEVICE		2
#define KEYWORD_IDENT			3
#define KEYWORD_CONSOLE		4
#define KEYWORD_HINTS			5

#define OPTION_MANDATORY	"mandatory"
#define OPTION_OPTIONAL		"optional"

struct KEYWORD {
	const char* keyword;
	int         value;
} keywords[] = {
	{ "arch",			KEYWORD_ARCH },
	{ "device",		KEYWORD_DEVICE },
	{ "ident",		KEYWORD_IDENT },
	{ "console",	KEYWORD_CONSOLE },
	{ "hints",		KEYWORD_HINTS },
	{ NULL,    	 KEYWORD_UNKNOWN }
};

struct ENTRY {
	const char*		value;
	const char*		source;
	int matched;
	struct ENTRY*	next;
};

struct ENTRY* devices = NULL;
struct ENTRY* files = NULL;
const char* architecture = NULL;
const char* ident = NULL;
const char* console = NULL;
const char* hints = NULL;

void
entry_add(struct ENTRY** root, const char* v)
{
	struct ENTRY* new_entry = (struct ENTRY*)malloc(sizeof(struct ENTRY));
	new_entry->value = strdup(v);
	new_entry->matched = 0;

	new_entry->next = NULL;
	if (*root == NULL) {
		*root = new_entry;
	} else {
		struct ENTRY* e = *root;
		while (e->next != NULL)
			e = e->next;
		e->next = new_entry;
	}
}

struct ENTRY*
find_device(struct ENTRY* root, char* line)
{
	char* current = line;

	while (line != NULL) {
		char* ptr = strchr(line, ' ');
		if (ptr != NULL) {
			*ptr++ = '\0';
		}

		/* We need to match 'line' */
		struct ENTRY* e = devices;
		while (e != NULL) {
			if (!strcasecmp(e->value, line))
				return e;
			e = e->next;
		}

		line = ptr;
	}

	return NULL;
}

void
usage()
{
	fprintf(stderr, "usage: config [-h?] name\n\n");
	fprintf(stderr, "   -h, -?       this help\n");
	exit(EXIT_FAILURE);
}

int
resolve_keyword(const char* keyword)
{
	struct KEYWORD* kw;
	for (kw = keywords; kw->keyword != NULL; kw++) {
		if (!strcasecmp(kw->keyword, keyword))
			return kw->value;
	}
	return kw->value;
}

void
parse_configfile(const char* fname)
{
	char line[255];
	int lineno = 0;

	FILE* f = fopen(fname, "rt");
	if (f == NULL)
		err(1, "fopen");

	while (fgets(line, sizeof(line), f) != NULL) {
		line[strlen(line) - 1] = '\0';
		lineno++;

		/* ignore empty lines and lines starting with a # */
		if (line[0] == '\n' || line[0] == '\0')
			continue;

		/* split the line in two space-seperated pieces, if possible */
		int keyword = KEYWORD_UNKNOWN;
		char* value = strchr(line, ' ');
		if (value == NULL)
			value = strchr(line, '\t');
		if (value != NULL) {
			*value++ = '\0';
			while (isspace(*value))
				value++;
			keyword = resolve_keyword(line);
		}

		switch (keyword) {
			case KEYWORD_ARCH:
				if (architecture != NULL)
					errx(1, "%s:%u: architecture re-specified (was %s)", fname, lineno, architecture);
				architecture = strdup(value);
				break;
			case KEYWORD_DEVICE:
				entry_add(&devices, value);
				break;
			case KEYWORD_IDENT:
				if (ident != NULL)
					errx(1, "%s:%u: identification re-specified (was %s)", fname, lineno, ident);
				ident = strdup(value);
				break;
			case KEYWORD_CONSOLE:
				if (console != NULL)
					errx(1, "%s:%u: console re-specified (was %s)", fname, lineno, console);
				console = strdup(value);
				break;
			case KEYWORD_HINTS:
				if (hints != NULL)
					errx(1, "%s:%u: hints re-specified (was %s)", fname, lineno, hints);
				hints = strdup(value);
				break;
			default:
				fprintf(stderr, "%s:%u: parse error\n", fname, lineno);
				exit(EXIT_FAILURE);
		}
	}

	fclose(f);
}

void
parse_devfile(const char* arch)
{
	char path[PATH_MAX];
	char line[255];
	int lineno = 0;
	FILE* f;

	if (arch != NULL)
		snprintf(path, sizeof(path), "../../../conf/files.%s", arch);
	else
		snprintf(path, sizeof(path), "../../../conf/files");
	f = fopen(path, "rt");
	if (f == NULL)
		err(1, "cannot open %s (is the architecture correct?)", path);

	while (fgets(line, sizeof(line), f) != NULL) {
		line[strlen(line) - 1] = '\0';
		lineno++;

		/* ignore empty lines and lines starting with a # */
		if (line[0] == '\n' || line[0] == '\0' || line[0] == '#')
			continue;

		/* change all tabs to spaces to simplify logic below */
		char* ptr;
		while ((ptr = strchr(line, '\t')) != NULL) {
			*ptr = ' ';
		}

		/* split the line in a filename -> options piece */
		char* options = strchr(line, ' ');
		if (options == NULL)
			errx(1, "%s:%u: parse error", path, lineno);

		*options++ = '\0';
		while (isspace(*options))
			options++;

		/*
		 * OK, there are two options: either it's optional, or it's not;
		 * corner-case the first case to keep the flow understandable.
		 */
		if (!strcasecmp(options, OPTION_MANDATORY)) {
			/* Must include */
			entry_add(&files, line);
			continue;
		}

		/* split the line in clause -> device piece */
		char* value = strchr(options, ' ');
		if (value == NULL)
			errx(1, "%s:%u: parse error", path, lineno);
		*value++ = '\0';
		/* We only support optional devices yet... */
		if (strcasecmp(options, OPTION_OPTIONAL) != 0) {
			errx(1, "%s:%u: parse error", path, lineno);
		}

		/*
		 * OK, we need to see whether a device in our kernel matches whatever
		 * is listed for this device.
		 */
		struct ENTRY* e = find_device(devices, value);
		if (e == NULL)
			continue;

		/* We have a match */
		e->matched = 1;
		e->source = strdup(line);
		entry_add(&files, line);
	}

	fclose(f);
}

char*
build_object(char* str) {
	char* ptr = strrchr(str, '/');
	if (ptr == NULL)
		ptr = str;
	else
		ptr++;

	/* We can only link objects */
	char* obj = strrchr(ptr, '.');
	if (obj != NULL) {
		obj++;
		*obj++ = 'o';
		*obj = '\0';
	}

	return ptr;
}

void
emit_objects(FILE* f, struct ENTRY* entrylist)
{
	char tmp[PATH_MAX];
	struct ENTRY* e;
	unsigned int n = 0;

	for (e = entrylist; e != NULL; e = e->next) {
		if (n > 0 && n % 10 == 0) {
			fprintf(f, " \\\n\t\t");
			n = 0;
		}

		/* Remove any path information */
		strncpy(tmp, e->value, sizeof(tmp));
		fprintf(f, " %s", build_object(tmp));
		n++;
	}
}

void
emit_compile(FILE* f, char* dst, const char* src) 
{
	char* ext = strrchr(src, '.');
	if (ext == NULL)
		err(1, "internal error: '%s' doesn't have an extension", src);
	ext++;

	switch(*ext) {
		case 'c':
			fprintf(f, "\t\t$(CC) $(CFLAGS) -c -o %s $S/%s\n", dst, src);
			break;
		case 'S':
			fprintf(f, "\t\t$(CC) $(CFLAGS) -DASM -c -o %s $S/%s\n", dst, src);
			break;
		default:
			err(1, "extension '%s' unsupported - help!", ext);
	}
}

void
emit_files(FILE* f, struct ENTRY* entrylist)
{
	char tmp[PATH_MAX];
	struct ENTRY* e;

	for (e = entrylist; e != NULL; e = e->next) {
		strncpy(tmp, e->value, sizeof(tmp));

		char* objname = build_object(tmp);
		fprintf(f, "%s:\t\t$S/%s\n", objname, e->value);
		emit_compile(f, objname, e->value);
		fprintf(f, "\n");
	}
}

void
create_makefile()
{
	char in_path[PATH_MAX];
	char out_path[PATH_MAX];
	char line[255];
	int lineno = 0;
	FILE *f_in, *f_out;

	snprintf(in_path, sizeof(in_path), "../../../conf/Makefile.%s", architecture);
	f_in = fopen(in_path, "rt");
	if (f_in == NULL)
		err(1, "cannot open %s (is the architecture correct?)", in_path);

	snprintf(out_path, sizeof(out_path), "../compile/%s", ident);
	mkdir(out_path, 0755); /* not fatal if this fails */
	snprintf(out_path, sizeof(out_path), "../compile/%s/Makefile", ident);
	f_out = fopen(out_path, "w");
	if (f_out == NULL)
		err(1, "cannot create %s", out_path);

	while (fgets(line, sizeof(line), f_in) != NULL) {
		line[strlen(line) - 1] = '\0';

		/* Lines starting with '%' are magic, so just copy-paste anything else */
		if (line[0] != '%') {
			fprintf(f_out, "%s\n", line);
			continue;
		}

		if (!strcasecmp(line + 1, "OBJS")) {
			fprintf(f_out, "OBJS=\t\t");
			emit_objects(f_out, files);
			fprintf(f_out, "\n\n");
			continue;
		}

		if (!strcasecmp(line + 1, "FILES")) {
			emit_files(f_out, files);
			continue;
		}

		errx(1, "unsupported macro '%s', aborting", line);
	}

	fclose(f_out);
	fclose(f_in);
}

void
check_ident()
{
	if (ident == NULL)
		errx(1, "no kernel identification provided");

	int i;
	for (i = 0; i < strlen(ident); i++) {
		if (isalnum(ident[i]) || ident[i] == '-' || ident[i] == '_')
			continue;

		errx(1, "kernel identification string '%s' must contain only alphanumeric/-/_ chars", ident);
	}
}

void
create_devprobe()
{
	char path[PATH_MAX];
	FILE* f;

	snprintf(path, sizeof(path), "../compile/%s/devprobe.inc", ident);
	f = fopen(path, "w");
	if (f == NULL)
		err(1, "cannot create %s", path);

	fprintf(f, "/* This file is automatically generated by config - do not edit! */\n\n");

	struct ENTRY* e = devices;
	for (e = devices; e != NULL; e = e->next) {
		fprintf(f, "extern struct PROBE probe_%s;\n", e->value);
	}
	fprintf(f, "\n");
	fprintf(f, "struct PROBE* devprobe[] = {\n");
	for (e = devices; e != NULL; e = e->next) {
		fprintf(f, "\t&probe_%s,\n", e->value);
	}
	fprintf(f, "\tNULL\n");
	fprintf(f, "};\n");

	fclose(f);
}

void
create_console()
{
	char path[PATH_MAX];
	FILE* f;

	if (console != NULL) {
		int found = 0;
		struct ENTRY* e;
		for (e = devices; e != NULL; e = e->next) {
			if (!strcmp(e->value, console)) {
				found++;
				break;
			}
		}

		if (!found)
			errx(1, "console driver '%s' does not exist", console);
	}

	snprintf(path, sizeof(path), "../compile/%s/console.inc", ident);
	f = fopen(path, "w");
	if (f == NULL)
		err(1, "cannot create %s", path);

	fprintf(f, "/* This file is automatically generated by config - do not edit! */\n\n");
	if (console != NULL)
		fprintf(f, "#define CONSOLE_DRIVER drv_%s\n", console);
	else
		fprintf(f, "/* #define CONSOLE_DRIVER */\n");

	fclose(f);
}

void
create_hints()
{
	char path[PATH_MAX];
	char line[LINE_MAX];
	char tmp_line[LINE_MAX];
	FILE* in;
	FILE* out;

	snprintf(path, sizeof(path), "../compile/%s/hints.inc", ident);
	out = fopen(path, "w");
	if (out == NULL)
		err(1, "cannot create %s", path);

	fprintf(out, "/* This file is automatically generated by config - do not edit! */\n\n");
	fprintf(out, "static const char* config_hints[] = {\n");
	if (hints != NULL) {
		in = fopen(hints, "r");
		if (in == NULL) {
			snprintf(path, sizeof(path), "../../../conf/%s", hints);
			in = fopen(path, "r");
			if (in == NULL)
				err(1, "cannot open %s nor %s", hints, path);
		}
		while (fgets(line, LINE_MAX, in) != NULL) {
			/* handle escaping of " and \ */
			char* in_ptr = line;
			char* out_ptr = tmp_line;
			while (*in_ptr) {
				switch(*in_ptr) {
					case '\n':
						/* ignore return */
						break;
					case '"':
					case '\\':
						*out_ptr++ = '\\';
					default:
						*out_ptr++ = *in_ptr;
						break;
				}
				in_ptr++;
			}
			*out_ptr = '\0';
			fprintf(out, "\t\"%s\",\n", tmp_line);
		}
		fclose(in);
	}
	fprintf(out, "\tNULL\n");
	fprintf(out, "};\n");
	fclose(out);
}

int
main(int argc, char* argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "?h")) != -1) {
		switch(ch) {
			case 'h':
			case '?':
				usage();
				/* NOTREACHED */
		}
	}
	argc -= optind; argv += optind;
	if (argc != 1)
		usage();

	parse_configfile(argv[0]);
	if (architecture == NULL)
		errx(1, "%s: no architecture specified", argv[0]);
	if (console == NULL)
		warnx("%s: no console specified; things will be quiet", argv[0]);
	if (hints == NULL)
		warnx("%s: no hints file specified; devices may not be found", argv[0]);

	check_ident();

	parse_devfile(architecture);
	parse_devfile(NULL);

	create_makefile();
	create_devprobe();
	create_console();
	create_hints();

	return 0;
}

/* vim:set ts=2 sw=2: */
