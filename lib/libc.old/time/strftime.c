#include <stdio.h>
#include <time.h>

size_t
strftime(char* buf, size_t maxsize, const char* format, const struct tm* timeptr)
{
	return snprintf(buf, maxsize, "strftime()");
}
