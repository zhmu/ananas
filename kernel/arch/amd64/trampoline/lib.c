#include <sys/types.h>

void*
memcpy(void* dst, const void* src, size_t len)
{
	char* target = (char*)dst;
	const char* source = (const char*)src;

	while (len--)
		*target++ = *source++;

	return dst;
}

void*
memset(void* dst, int c, size_t len)
{
	int* target = (int*)dst;
	while (len--)
		*target++ = c;
	return dst;
}

/* vim:set ts=2 sw=2: */
