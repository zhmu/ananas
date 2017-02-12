#include <ananas/lib.h>
#include <ananas/mm.h>

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
	while(1) {
		if (*s == c)
			return (char*)s;
		if (*s == '\0')
			break;
		s++;
	}
	return NULL;
}

char*
strrchr(const char* s, int c)
{
	char* ptr = (char*)(s + strlen(s) - 1);
	while (ptr >= s) {
		if (*ptr == c)
			return (char*)s;
		ptr--;
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
	auto c1 = static_cast<const char*>(s1);
	auto c2 = static_cast<const char*>(s2);

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
	} else if (base == 0) {
		base = 10;
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
		break;
	}
	if (endptr != NULL)
		*endptr = (char*)ptr;
	return val;
}

char*
strcat(char* dst, const char* src)
{
	char* org_dest = dst;

	while (*dst != '\0') dst++;

	while (*src != '\0')
		*dst++ = *src++;
	*dst = '\0';

	return org_dest;
}

char*
strdup(const char* s)
{
	auto ptr = static_cast<char*>(kmalloc(strlen(s) + 1));
	if (ptr == NULL) return NULL;
	strcpy(ptr, s);
	return ptr;
}

char*
strncpy(char* dst, const char* src, size_t n)
{
	char* ptr = dst;

	while (*src != '\0' && n > 0) {
		*ptr++ = *src++; n--;
	}
	while(n > 0) {
		*ptr++ = '\0'; n--;
	}
	return dst;
}

/* vim:set ts=2 sw=2: */
