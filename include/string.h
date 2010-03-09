#ifndef __STRING_H__
#define __STRING_H__

#include <machine/_types.h>
#include <sys/_null.h>

char* strcpy(char* dst, const char* src);
void* memcpy(void* dest, const void* source, size_t len);
void* memset(void* b, int c, size_t len);
size_t strlen(const char* s);
int memcmp(const void* s1, const void* s2, size_t len);
int strcmp(const char* s1, const char* s2);
char* strchr(const char* s, int c);
char* strcat(char* dst, const char* src);
char* strncat(char* dst, const char* src, size_t n);
char* strncpy(char* dst, const char* src, size_t n);
void* memchr(const void* src, int c, size_t len);
char* strstr(const char* large, const char* small);
size_t strcspn(const char* s1, const char* s2);
char* strpbrk(const char* s1, const char* s2);
int strcoll(const char* s1, const char* s2);
double strtod(const char* ptr, char** endptr);

#endif /* __STRING_H__ */
