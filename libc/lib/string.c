#include "types.h"

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
		/* what's this? */
		break;
	}
	if (endptr != NULL)
		*endptr = (char*)ptr;
	return val;
}

char*
strcat(char* dst, const char* src)
{
	char* ptr = dst;
	while (*ptr != '\0') ptr++;
	while (*src != '\0') *ptr++ = *src++;
	*ptr = '\0';
	return dst;
}

char*
strncat(char* dst, const char* src, size_t n)
{
	char* ptr = dst;
	while (*ptr != '\0') ptr++;

	while (*src != '\0' && n > 0) { *ptr++ = *src++; n--; }
	*ptr = '\0';
	return dst;
}

char*
strncpy(char* dst, const char* src, size_t n)
{
	char* ptr = dst;

	while (*src && n > 0) {
		*ptr++ = *src++; n--;
	}
	if (n > 0)
		*ptr = '\0';
	return dst;
}

void*
memchr(const void* src, int c, size_t len)
{
	uint8_t* ptr = (uint8_t*)src;
	while (len-- > 0) {
		if (*ptr == c)
			return ptr;
		ptr++;
	}
	return NULL;
}

char*
strstr(const char* large, const char* small)
{
	const char* s = large;

	while (*s != '\0') {
		if (*s != *small) {
			s++;
			continue;
		}
		/* First char matches; try the rest */
		const char* q = s;
		const char* find = small;
		while (*q != '\0' && *find != '\0' && *q == *find) { q++; find++; }
		if (*q == '\0' && *find == '\0')
			return (char*)s;
		s++;
	}
	return NULL;
}

size_t
strcspn(const char* s1, const char* s2)
{
	const char* ptr = s1;
	while (*ptr != '\0') {
		const char* search = s2;
		while (*search != '\0' && *ptr != *search)
			search++;
		if (*search != '\0')
			break;
	}
	return (ptr - s1);
}

char*
strpbrk(const char* s1, const char* s2)
{
	for (; *s1 != '\0'; s1++)
		for (const char *search = s2; search != '\0'; search++)
			if (*search == *s1)
				return (char*)s1;
	return NULL;
}

int
strcoll(const char* s1, const char* s2)
{
	return strcmp(s1, s2);
}

double
strtod(const char* ptr, char** endptr)
{
	return (double)strtoul(ptr, endptr, 10);
}

/* vim:set ts=2 sw=2: */
