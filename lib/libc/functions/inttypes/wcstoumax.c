#include <stddef.h>
#include <inttypes.h>
#include <stdlib.h>

uintmax_t
wcstoumax(const wchar_t* ptr, wchar_t** endptr, int base)
{
	size_t len = wcstombs(NULL, ptr, 0);
	if (len == (size_t)-1) {
		if (endptr != NULL)
			*endptr = (wchar_t*)ptr;
		return 0.0;
	}

	char* buf = malloc(len + 1);
	if (buf == NULL) {
		if (endptr != NULL)
			*endptr = (wchar_t*)ptr;
		return 0.0;
	}

	wcstombs(buf, ptr, len);

	char* endp;
	uintmax_t d = strtoumax(buf, &endp, base);
	if (endptr != NULL) {
		*endptr = (wchar_t*)ptr + (endp - buf);
	}

	free(buf);
	return d;
}
