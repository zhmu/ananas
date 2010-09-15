#include <unistd.h>
#include <string.h>

int optind = 1; /* option to be processed */
int opterr;
int optopt;
int optreset = 0;
char* optarg = NULL;

int
getopt(int argc, char* const argv[], const char* optstring)
{
	if (optreset) {
		optind = 1;
		optreset = 0;
	}

	for (int i = optind; i < argc; i++) {
		const char* arg = argv[i];
		if (arg == NULL || *arg != '-' || strcmp(arg, "-") == 0)
			return -1;
		if (strcmp(arg, "--") == 0) {
			optind++;
			return -1;
		}

		/* ok, we have an option - try to see if it matches! */
		optind++;
		const char* opt = optstring;
		for (; *opt != '\0'; opt++)
			if (*opt == *arg)
				break;
		if (*opt == '\0') {
			/* option not found */
			return '?';
		}

		/* option matched - determine if this needs an arg */
		if (*(opt + 1) == ':') {
			if (optind + 1 < argc) {
				/* need an arg, but can't find */
				return '?';
			}
			optarg = argv[++optind];
			return *opt;
		}

		/* just a flag */
		optind++;
		return *opt;
	}
	return -1;
}

/* vim:set ts=2 sw=2: */
