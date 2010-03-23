#include <sys/types.h>
#include <stdarg.h>

#ifndef __LIBKERN_H__
#define __LIBKERN_H__

#define KASSERT(x, msg, args...) \
	if (!(x)) \
		panic("%s: "msg"\n", __func__, ## args)


void* memcpy(void* dst, const void* src, size_t len);
void* memset(void* b, int c, size_t len);
void vaprintf(const char* fmt, va_list ap);
void kprintf(const char* fmt, ...);
void panic(const char* fmt, ...);
int sprintf(char* str, const char* fmt, ...);
char* strdup(const char* s);

char* strcpy(char* dst, const char* src);
int   strcmp(const char* s1, const char* s2);
int   strncmp(const char* s1, const char* s2, size_t n);
char* strchr(const char* s, int c);
size_t strlen(const char* s);
char* strcat(char* dst, const char* src);

char* strncpy(char* dst, const char* src, size_t n);

int memcmp(const void* s1, const void* s2, size_t len);

unsigned long strtoul(const char* ptr, char** endptr, int base);

void abort();

#endif /* __LIBKERN_H__ */
