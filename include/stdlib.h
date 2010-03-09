#include <malloc.h>

#ifndef __STDLIB_H__
#define __STDLIB_H__

#include <sys/_null.h>

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

void abort();
unsigned long strtoul(const char* ptr, char** endptr, int base);

#endif /* __STDLIB_H__ */
