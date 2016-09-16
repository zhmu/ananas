#include <stdlib.h>

long double strtold(const char *nptr, char **endptr)
{
	/* XXX this is a hack */
	return (long double)strtod(nptr, endptr);
}
