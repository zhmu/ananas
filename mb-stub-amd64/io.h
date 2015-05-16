#ifndef __IO_H__
#define __IO_H__

#include "types.h"

int putchar(int ch);
int puts(const char* s);
void putx(uint32_t v);
int printf(const char* fmt, ...);
void panic(const char* fmt, ...) __attribute__((noreturn));

#endif /* __IO_H__ */
