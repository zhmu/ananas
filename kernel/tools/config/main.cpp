#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "../../../include/ananas/queue.h"

/* XXX This is a kludge, but PATH_MAX need not be defined */
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

enum TOKEN {
	UNKNOWN,
	ARCH, DEVICE, IDENT,
	MANDATORY,
	OPTIONAL, OPTION,
	INCLUDE, AND, OR
};

struct TOKENS {
	const char* token;
	enum TOKEN  value;
} tokens[] = {
	{ "arch",      ARCH },
	{ "device",    DEVICE },
	{ "ident",     IDENT },
	{ "mandatory", MANDATORY },
	{ "optional",  OPTIONAL },
	{ "option",    OPTION },
	{ "include",   INCLUDE },
	{ "&&", 		   AND },
	{ "||", 		   OR },
	{ NULL,        UNKNOWN }
};

struct ENTRY {
	const char*		value;
	const char*		source;
	int matched;
	QUEUE_FIELDS(struct ENTRY);
};

QUEUE_DEFINE(ENTRY_LIST, struct ENTRY);

static struct ENTRY_LIST devices;
static struct ENTRY_LIST options;
static struct ENTRY_LIST files;
static const char* architecture = NULL;
static const char* ident = NULL;

static struct ENTRY*
entry_make(char* value)
{
	struct ENTRY* e = new ENTRY;
	e->value = strdup(value);
	e->matched = 0;
	return e;
}

/*
 * Locate entry 'line' in the chained-list of entries
 */
static struct ENTRY*
find_entry(struct ENTRY_LIST* list, char* line)
{
	while (line != NULL) {
		char* ptr = strchr(line, ' ');
		if (ptr != NULL) {
			*ptr++ = '\0';
		}

		/* We need to match 'line' */
		QUEUE_FOREACH(list, e, struct ENTRY) {
			if (!strcasecmp(e->value, line))
				return e;
		}
		line = ptr;
	}

	return NULL;
}

/*
 * Checks entry list root to ensure all entries have been matched.
 */
static void
check_entries(struct ENTRY_LIST* list, const char* type)
{
	QUEUE_FOREACH(list, e, struct ENTRY) {
		if (!e->matched)
			errx(1, "unknown %s '%s', aborting", type, e->value);
	}
}

static void
usage()
{
	fprintf(stderr, "usage: config [-h?] name\n\n");
	fprintf(stderr, "   -h, -?       this help\n");
	exit(EXIT_FAILURE);
}

/*
 * Classifies string 'token' to a token.
 */
static enum TOKEN
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
static void
parse_configfile(FILE* f, const char* fname)
{
	char line[255];
	int lineno = 0;

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
			case DEVICE: {
				struct ENTRY* e = entry_make(value);
				QUEUE_ADD_TAIL(&devices, e);
				break;
			}
			case IDENT:
				if (ident != NULL)
					errx(1, "%s:%u: identification re-specified (was %s)", fname, lineno, ident);
				ident = strdup(value);
				break;
			case OPTION: {
				struct ENTRY* e = entry_make(value);
				QUEUE_ADD_TAIL(&options, e);
				break;
			}
			case INCLUDE: {
				FILE* incfile = fopen(value, "rb");
				if (incfile == NULL)
					err(1, "fopen");
				parse_configfile(incfile, value);
				fclose(incfile);
				break;
			}
			default:
				errx(1 , "%s:%u: parse error", fname, lineno);
		}
	}
}

static void
parse_devfile(const char* arch)
{
	char path[PATH_MAX];

	snprintf(path, sizeof(path), "../../../conf/files");
	if (arch != NULL) {
		strncat(path, ".", sizeof(path));
		strncat(path, arch, sizeof(path));
	}
	FILE* f = fopen(path, "rt");
	if (f == NULL)
		err(1, "cannot open %s (is the architecture correct?)", path);

	char line[255];
	int lineno = 0;
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

		/* Special-case 'include' here as it has nothing to do with the stuff below */
		if (strcmp(line, "include") == 0) {
			parse_devfile(opts);
			continue;
		}

		/*
		 * OK, there are two options: either it's optional, or it's not;
		 * corner-case the first case to keep the flow understandable.
		 */
		enum TOKEN token = resolve_token(opts);
		if (token == MANDATORY) {
			/* Must include */
			struct ENTRY* e = entry_make(line);
			QUEUE_ADD_TAIL(&files, e);
			continue;
		}

		/* Handle all pieces; they can be combined using && and || */
		char* next_token = opts;
		int match = 0;
		while(opts != NULL) {
			/* split the line in clause -> device piece */
			char* value = strchr(opts, ' ');
			if (value == NULL)
				errx(1, "%s:%u: parse error (%s)", path, lineno, next_token);
			*value++ = '\0';

			/*
			 * Resolve the token here; it must be done before dissecting the input
			 * futher so we can handle && and ||.
			 */
			token = resolve_token(opts);
			switch(token) {
				case AND:
					/*
					 * Both conditions must be true; so if the previous one isn't, bail.
					 */
					opts = (match ? value : NULL);
					continue;
				case OR:
					/* Either one condition suffices, so if one is already matched, bail */
					opts = (!match ? value : NULL);
					continue;
				default:
					break;
			}

			next_token = strchr(value, ' ');
			if (next_token != NULL)
				*next_token++ = '\0';

			struct ENTRY* e;
			switch(token) {
				case OPTIONAL: {
					/*
					 * OK, we need to see whether a device in our kernel matches whatever
					 * is listed for this device.
					 */
					e = find_entry(&devices, value);
					if (e != NULL) {
						e->matched = 1;
						e->source = strdup(line);
						match = 1;
					}
					break;
				}
				case OPTION: {
					e = find_entry(&options, value);
					if (e != NULL) {
						e->matched = 1;
						e->source = strdup(line);
						match = 1;
					}
					break;
				}
				default:
					errx(1, "%s:%u: parse error", path, lineno);
					break;
			}

			opts = next_token;
		}

		if (match) {
			/* We have a match */
			struct ENTRY* new_entry = entry_make(line);
			QUEUE_ADD_TAIL(&files, new_entry);
		}
	}

	fclose(f);
}

static char*
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

static void
emit_objects(FILE* f, struct ENTRY_LIST* list)
{
	char tmp[PATH_MAX];
	unsigned int n = 0;

	QUEUE_FOREACH(list, e, struct ENTRY) {
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

static void
emit_compile(FILE* f, char* dst, const char* src) 
{
	const char* ext = strrchr(src, '.');
	if (ext == NULL)
		err(1, "internal error: '%s' doesn't have an extension", src);
	ext++;

	if (strcmp(ext, "c") == 0) {
		fprintf(f, "\t\t$(CC) $(CFLAGS) -c -o %s $S/%s\n", dst, src);
	} else if (strcmp(ext, "cpp") == 0) {
		fprintf(f, "\t\t$(CXX) $(CXXFLAGS) -c -o %s $S/%s\n", dst, src);
	} else if (strcmp(ext, "S") == 0) {
		fprintf(f, "\t\t$(CC) $(CFLAGS) -DASM -c -o %s $S/%s\n", dst, src);
	} else {
		err(1, "extension '%s' unsupported - help!", ext);
	}
}

static const char*
extra_deps(const char* src) 
{
	const char* ext = strrchr(src, '.');
	if (ext == NULL)
		err(1, "internal error: '%s' doesn't have an extension", src);
	ext++;

	switch(*ext) {
		case 'c':
			return "";
		case 'S':
			return " asmsyms.h";
	}
	err(1, "extension '%s' unsupported - help!", ext);
}

static void
emit_files(FILE* f, struct ENTRY_LIST* list)
{
	char tmp[PATH_MAX];

	QUEUE_FOREACH(list, e, struct ENTRY) {
		strncpy(tmp, e->value, sizeof(tmp));

		char* objname = build_object(tmp);
		fprintf(f, "%s:\t\t$S/%s%s\n", objname, e->value, extra_deps(e->value));
		emit_compile(f, objname, e->value);
		fprintf(f, "\n");
	}
}

static void
create_makefile()
{
	char in_path[PATH_MAX];
	char out_path[PATH_MAX];
	char line[255];
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
			emit_objects(f_out, &files);
			fprintf(f_out, "\n\n");
			continue;
		}

		if (!strcasecmp(line + 1, "FILES")) {
			emit_files(f_out, &files);
			continue;
		}

		errx(1, "unsupported macro '%s', aborting", line);
	}

	fclose(f_out);
	fclose(f_in);
}

static void
check_ident()
{
	if (ident == NULL)
		errx(1, "no kernel identification provided");

	for (unsigned int i = 0; i < strlen(ident); i++) {
		if (isalnum(ident[i]) || ident[i] == '-' || ident[i] == '_')
			continue;

		errx(1, "kernel identification string '%s' must contain only alphanumeric/-/_ chars", ident);
	}
}

static void
create_options()
{
	char path[PATH_MAX];
	FILE* f;

	snprintf(path, sizeof(path), "../compile/%s/options.h", ident);
	f = fopen(path, "w");
	if (f == NULL)
		err(1, "cannot create %s", path);

	fprintf(f, "/* This file is automatically generated by config - do not edit! */\n\n");

	fprintf(f, "#define ARCHITECTURE \"%s\"\n", architecture);

	QUEUE_FOREACH(&options, e, struct ENTRY) {
		fprintf(f, "#define OPTION_%s\n", e->value);
	}
	fclose(f);
}

static void
create_symlink()
{
	char src_path[PATH_MAX];
	char dst_path[PATH_MAX];

	snprintf(src_path, sizeof(src_path), "../../../../../include/ananas/%s", architecture);
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

	QUEUE_INIT(&devices);
	QUEUE_INIT(&options);
	QUEUE_INIT(&files);

	FILE* cfgfile = fopen(argv[0], "rb");
	if (cfgfile == NULL)
		err(1, "fopen");
	parse_configfile(cfgfile, argv[0]);
	fclose(cfgfile);

	if (architecture == NULL)
		errx(1, "%s: no architecture specified", argv[0]);

	check_ident();

	parse_devfile(architecture);
	parse_devfile(NULL);

	check_entries(&devices, "device");
	check_entries(&options, "option");

	create_makefile();
	create_options();
	create_symlink();

	return 0;
}

/* vim:set ts=2 sw=2: */
