#include <wchar.h>
#include <stdlib.h>

float
wcstof(const wchar_t* ptr, wchar_t** endptr)
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
	float f = strtof(buf, &endp);
	if (endptr != NULL) {
		*endptr = (wchar_t*)ptr + (endp - buf);
	}

	free(buf);
	return f;
}


