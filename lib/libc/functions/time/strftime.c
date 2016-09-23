#include <time.h>
#include <stdio.h>

#ifndef REGTEST
size_t strftime(char *s, size_t maxsize, const char *format, const struct tm *timptr)
{
	/* XXX implement me */
	return snprintf(s, maxsize, "[strftime todo]");
}
#endif
