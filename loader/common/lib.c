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

void*
memset(void* b, int c, size_t len)
{
	char* ptr = (char*)b;
	while (len--) {
		*ptr++ = c;
	}
	return b;
}

char*
strcpy(char* dst, const char* src)
{
	char* s = dst;

	while (*src) {
		*dst++ = *src++;
	}
	*dst = '\0';
	return s;
}

int
strcmp(const char* s1, const char* s2)
{
	while (*s1 != '\0' && *s2 != '\0' && *s1 == *s2) {
		s1++; s2++;
	}
	return *s1 - *s2;
}

int
strncmp(const char* s1, const char* s2, size_t n)
{
	while (n > 0 && *s1 != '\0' && *s2 != '\0' && *s1 == *s2) {
		n--; s1++; s2++;
	}
	return (n == 0) ? n : *s1 - *s2;
}

char*
strchr(const char* s, int c)
{
	while (1) {
		if (*s == c)
			return (char*)s;
		if (*s == '\0')
			break;
		s++;
	}
	return NULL;
}

size_t
strlen(const char* s)
{
	size_t len = 0;

	for (; *s != '\0'; s++, len++);
	return len;
}
