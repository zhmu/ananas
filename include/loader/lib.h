#ifndef __LOADER_LIB_H__
#define __LOADER_LIB_H__

#include <ananas/types.h>

/* io.c */
int puts(const char* s);
int printf(const char* fmt, ...);
int sprintf(char* str, const char* fmt, ...);

/* lib.c */
void* memcpy(void* dest, const void* src, size_t len);
void* memset(void* b, int c, size_t len);
char* strcpy(char* dst, const char* src);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
char* strchr(const char* s, int c);
size_t strlen(const char* s);
int memcmp(const void* s1, const void* s2, size_t len);
long int strtol(const char *nptr, char **endptr, int base);

#endif /* __LOADER_LIB_H__ */
