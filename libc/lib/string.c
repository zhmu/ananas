#include "lib.h"

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

char*
strchr(const char* s, int c)
{
	while (*s) {
		if (*s == c)
			return (char*)s;
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

int
memcmp(const void* s1, const void* s2, size_t len)
{
	const char* c1 = s1;
	const char* c2 = s2;

	while (len > 0 && *c1 == *c2) {
		len--; c1++; c2++;
	}
	return len > 0 ? *c1 - *c2 : 0;
}

unsigned long
strtoul(const char* ptr, char** endptr, int base)
{
	unsigned long val = 0;

	/* detect 0x hex modifier */
	if (base == 0 && *ptr == '0' && *(ptr + 1) == 'x') {
		base = 16; ptr += 2;
	}
	while (*ptr) {
		if (*ptr >= '0' && *ptr <= '9') {
			val = (val * base) + (*ptr - '0');
			ptr++;
			continue;
		}
		if (base == 0x10 && (*ptr >= 'a' && *ptr <= 'f')) {
		val = (val * base) + (*ptr - 'a') + 0xa;
			ptr++;
			continue;
		}
		if (base == 0x10 && (*ptr >= 'A' && *ptr <= 'F')) {
		val = (val * base) + (*ptr - 'A') + 0xa;
			ptr++;
			continue;
		}
		if (endptr != NULL)
			*endptr = (char*)ptr;
		break;
	}
	return val;
}

/* vim:set ts=2 sw=2: */
