#include "types.h"
#include "stdarg.h"

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

char* strcpy(char* dst, const char* src);
int   strcmp(const char* s1, const char* s2);

#endif /* __LIBKERN_H__ */
