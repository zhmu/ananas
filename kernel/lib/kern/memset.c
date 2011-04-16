#include <ananas/types.h>

void*
memset(void* b, int c, size_t len)
{
	char* ptr = (char*)b;
	while (len--) {
		*ptr++ = c;
	}
	return b;
}

/* vim:set ts=2 sw=2: */
