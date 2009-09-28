#include "types.h"

void*
memcpy(void* dest, const char* src, size_t len)
{
	char* d = (char*)dest;
	char* s = (char*)src;
	while (len--) {
		*d++ = *s++;
	}
	return dest;
}

void*
memset(void* b, int c, size_t len)
{
	char* ptr = (char*)b;
	while (len--) {
		*ptr++ = c;
	}
}

/* vim:set ts=2 sw=2: */
