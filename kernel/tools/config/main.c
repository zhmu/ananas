#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum TOKEN {
	UNKNOWN,
	ARCH, DEVICE, IDENT,
	CONSOLE, HINTS, MANDATORY,
	OPTIONAL, OPTION
};

struct TOKENS {
	const char* token;
	enum TOKEN  value;
} tokens[] = {
	{ "arch",      ARCH },
	{ "device",    DEVICE },
	{ "ident",     IDENT },
	{ "console",   CONSOLE },
	{ "hints",     HINTS },
	{ "mandatory", MANDATORY },
	{ "optional",  OPTIONAL },
	{ "option",		 OPTION },
	{ NULL,        UNKNOWN }
};

struct ENTRY {
	const char*		value;
	const char*		source;
	int matched;
	struct ENTRY*	next;
};

struct ENTRY* devices = NULL;
struct ENTRY* options = NULL;
struct ENTRY* files = NULL;
const char* architecture = NULL;
const char* ident = NULL;
const char* console = NULL;
const char* hints = NULL;

/*
 * Adds an entry 'v' to the chained list of 'root'. This will preserve order;
 * newer entries are always added last.
 */
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

/*
 * Locate entry 'line' in the chained-list of entries
 */
struct ENTRY*
find_entry(struct ENTRY* root, char* line)
{
	char* current = line;

	while (line != NULL) {
		char* ptr = strchr(line, ' ');
		if (ptr != NULL) {
			*ptr++ = '\0';
		}

		/* We need to match 'line' */
		struct ENTRY* e = root;
		while (e != NULL) {
			if (!strcasecmp(e->value, line))
				return e;
			e = e->next;
		}

		line = ptr;
	}

	return NULL;
}

/*
 * Checks entry list root to ensure all entries have been matched.
 */
void
check_entries(struct ENTRY* root, const char* type)
{
	struct ENTRY* e = root;

	while (e != NULL) {
		if (!e->matched)
			errx(1, "unknown %s '%s', aborting", type, e->value);
		e = e->next;
	}
}

void
usage()
{
	fprintf(stderr, "usage: config [-h?] name\n\n");
	fprintf(stderr, "   -h, -?       this help\n");
	exit(EXIT_FAILURE);
}

/*
 * Classifies string 'token' to a token.
 */
enum TOKEN
resolve_token(const char* token)
{
	struct TOKENS* tk;
	for (tk = tokens; tk->token != NULL; tk++) {
		if (!strcasecmp(tk->token, token))
			return tk->value;
	}
	return tk->value;
}

/*
 * Parses a kernel configuration file.
 */
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
		if (line[0] == '\n' || line[0] == '\0' || line[0] == '#')
			continue;

		/* split the line in two space-seperated pieces, if possible */
		enum TOKEN token = UNKNOWN;
		char* value = strchr(line, ' ');
		if (value == NULL)
			value = strchr(line, '\t');
		if (value != NULL) {
			*value++ = '\0';
			while (isspace(*value))
				value++;
			token = resolve_token(line);
		}

		switch (token) {
			case ARCH:
				if (architecture != NULL)
					errx(1, "%s:%u: architecture re-specified (was %s)", fname, lineno, architecture);
				architecture = strdup(value);
				break;
			case DEVICE:
				entry_add(&devices, value);
				break;
			case IDENT:
				if (ident != NULL)
					errx(1, "%s:%u: identification re-specified (was %s)", fname, lineno, ident);
				ident = strdup(value);
				break;
			case CONSOLE:
				if (console != NULL)
					errx(1, "%s:%u: console re-specified (was %s)", fname, lineno, console);
				console = strdup(value);
				break;
			case HINTS:
				if (hints != NULL)
					errx(1, "%s:%u: hints re-specified (was %s)", fname, lineno, hints);
				hints = strdup(value);
				break;
			case OPTION:
				entry_add(&options, value);
				break;
			default:
				errx(1 , "%s:%u: parse error", fname, lineno);
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
		char* opts = strchr(line, ' ');
		if (opts == NULL)
			errx(1, "%s:%u: parse error", path, lineno);

		*opts++ = '\0';
		while (isspace(*opts))
			opts++;

		/*
		 * OK, there are two options: either it's optional, or it's not;
		 * corner-case the first case to keep the flow understandable.
		 */
		enum TOKEN token = resolve_token(opts);
		if (token == MANDATORY) {
			/* Must include */
			entry_add(&files, line);
			continue;
		}

		/* split the line in clause -> device piece */
		char* value = strchr(opts, ' ');
		if (value == NULL)
			errx(1, "%s:%u: parse error", path, lineno);
		*value++ = '\0';

		token = resolve_token(opts);
		struct ENTRY* e;
		switch(token) {
			case OPTIONAL:
				/*
				 * OK, we need to see whether a device in our kernel matches whatever
				 * is listed for this device.
				 */
				e = find_entry(devices, value);
				if (e == NULL)
					continue;

				/* We have a match */
				e->matched = 1;
				e->source = strdup(line);
				entry_add(&files, line);
				break;
			case OPTION:
				e = find_entry(options, value);
				if (e == NULL)
					continue;

				/* We have a match */
				e->matched = 1;
				e->source = strdup(line);
				entry_add(&files, line);
				break;
			default:
				errx(1, "%s:%u: parse error", path, lineno);
				break;
			}
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
	char* bus = NULL;
	char* console_dev = NULL;

	if (console != NULL) {
		int found = 0;

		/*
		 * If the console contains a '.' inside the name, this indicates
		 * the bus it's attached on. This is needed for the hint.
		 */
		char* ptr = strrchr(console, '.');
		if (ptr != NULL) {
			*ptr++ = '\0';
			console_dev = ptr;
			bus = (char*)console;
		} else {
			console_dev = (char*)console;
			bus = NULL;
		}

		if (find_entry(devices, console_dev) == NULL)
			errx(1, "console driver '%s' does not exist", console_dev);
		if (bus != NULL && find_entry(devices, bus) == NULL)
			errx(1, "console bus '%s' does not exist", bus);
	}

	snprintf(path, sizeof(path), "../compile/%s/console.inc", ident);
	f = fopen(path, "w");
	if (f == NULL)
		err(1, "cannot create %s", path);

	fprintf(f, "/* This file is automatically generated by config - do not edit! */\n\n");
	if (console != NULL)
		fprintf(f, "#define CONSOLE_DRIVER drv_%s\n", console_dev);
	else
		fprintf(f, "/* #define CONSOLE_DRIVER */\n");
	if (bus != NULL)
		fprintf(f, "#define CONSOLE_BUS \"%s\"\n", bus);
	else
		fprintf(f, "/* #define CONSOLE_BUS */\n");

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
	fprintf(out, "const char* config_hints[] = {\n");
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

void
create_options()
{
	char path[PATH_MAX];
	FILE* f;

	snprintf(path, sizeof(path), "../compile/%s/options.h", ident);
	f = fopen(path, "w");
	if (f == NULL)
		err(1, "cannot create %s", path);

	fprintf(f, "/* This file is automatically generated by config - do not edit! */\n\n");

	struct ENTRY* e;
	for (e = options; e != NULL; e = e->next) {
		/* Remove any path information */
		fprintf(f, "#define %s\n", e->value);
	}
	fclose(f);
}

void
create_symlink()
{
	char src_path[PATH_MAX];
	char dst_path[PATH_MAX];

	snprintf(src_path, sizeof(src_path), "../../../../../include/%s", architecture);
	snprintf(dst_path, sizeof(dst_path), "../compile/%s/machine", ident);
	if (symlink(src_path, dst_path) < 0 && errno != EEXIST)
		err(1, "symlink");
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

	check_entries(devices, "device");
	check_entries(options, "option");

	create_makefile();
	create_devprobe();
	create_console();
	create_hints();
	create_options();
	create_symlink();

	return 0;
}

/* vim:set ts=2 sw=2: */
