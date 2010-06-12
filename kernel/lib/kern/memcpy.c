#include <sys/types.h>

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
