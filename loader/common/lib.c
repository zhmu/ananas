#include <ananas/types.h>
#include <loader/lib.h>

void*
memcpy(void* dest, const void* src, size_t len)
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

long int
strtol(const char *nptr, char **endptr, int base)
{
	if (base < 2 || base > 16)
		return -1;

	/* If no base given, make a guess */
	if (base == 0) {
		if (nptr[0] == '0' && ((nptr[1] == 'x') || (nptr[1] == 'X'))) {
			base = 16; nptr += 2;
		} else
			base = 10;
	}

	long int result = 0;
	while(*nptr != '\0') {
		int digit;
		if (*nptr >= '0' && *nptr <= '9')
			digit = *nptr - '0';
		else if (*nptr >= 'a' && *nptr <= 'f')
			digit = *nptr - 'a';
		else if (*nptr >= 'A' && *nptr <= 'F')
			digit = *nptr - 'A';
		else
			break; /* what's this? */

		if (digit >= base)
			break; /* digit not in range */

		result *= base;
		result += digit;
		nptr++;
	}

	if (endptr != NULL)
		*endptr = (char*)nptr;
	return result;
}

/* vim:set ts=2 sw=2: */
