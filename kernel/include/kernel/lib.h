#ifndef __LIBKERN_H__
#define __LIBKERN_H__

#include <ananas/types.h>
#include "kernel/cdefs.h"

#define KASSERT(x, msg, args...) \
    if (!(x))                    \
    _panic(__FILE__, __func__, __LINE__, msg, ##args)

#define panic(msg, args...) _panic(__FILE__, __func__, __LINE__, msg, ##args)

/* Rounds up 'n' to a multiple of 'mult' */
#define ROUND_UP(n, mult) (((n) % (mult) == 0) ? (n) : (n) + ((mult) - (n) % (mult)))

/* Rounds down 'n' to a multiple of 'mult' */
#define ROUND_DOWN(n, mult) (((n) % (mult) == 0) ? (n) : (n) - (n) % (mult))

#ifdef __cplusplus
// Placement new declaration
void* operator new(size_t, void*) throw();

extern "C" {
#endif
void* memcpy(void* dst, const void* src, size_t len) __nonnull;
void* memset(void* b, int c, size_t len) __nonnull;
void vaprintf(const char* fmt, va_list ap);
int vsnprintf(char* str, size_t len, const char* fmt, va_list ap);
void kprintf(const char* fmt, ...);
void _panic(const char* file, const char* func, int line, const char* fmt, ...) __noreturn;
int sprintf(char* str, const char* fmt, ...);
int snprintf(char* str, size_t len, const char* fmt, ...);
char* strdup(const char* s) __nonnull;

char* strcpy(char* dst, const char* src) __nonnull;
int strcmp(const char* s1, const char* s2) __nonnull;
int strncmp(const char* s1, const char* s2, size_t n) __nonnull;
char* strchr(const char* s, int c) __nonnull;
char* strrchr(const char* s, int c) __nonnull;
size_t strlen(const char* s) __nonnull;
char* strcat(char* dst, const char* src) __nonnull;

char* strncpy(char* dst, const char* src, size_t n) __nonnull;

int memcmp(const void* s1, const void* s2, size_t len) __nonnull;

unsigned long strtoul(const char* ptr, char** endptr, int base);

/* Crude, but our memcpy() should be able to handle overlapping regions */
#define memmove memcpy

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* __LIBKERN_H__ */
