/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef LIB_H
#define LIB_H

#include <sys/types.h>

using register_t = long;

#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define assert(x) (!(x) ? panic("RTLD ASSERTION FAILED: " #x) : (void)0)

#define die(x...)  \
    do {           \
        printf(x); \
        exit(-1);  \
    } while (0)

ssize_t write(int fd, const void* buf, size_t count);
void panic(const char* msg);
char* strcpy(char* d, const char* s);
char* strncpy(char* d, const char* s, size_t n);
size_t strlen(const char* s);
char* strchr(const char* s, int c);
char* strcat(char* dest, const char* src);
int strcmp(const char* a, const char* b);
int strncmp(const char* s1, const char* s2, size_t n);
char* strdup(const char* s);

void* memset(void* s, int c, size_t n);
void memcpy(void* d, const void* s, size_t n);

extern "C" int open(const char* path, int flags, ...);
extern "C" int close(int fd);
ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
off_t lseek(int fd, off_t offset, int whence);
void exit(int status);

int printf(const char* fmt, ...);
int sprintf(char* str, const char* fmt, ...);

void InitializeFromStack(register_t* stk);
const char* GetProgName();
char* getenv(const char* name);

// From malloc.cpp */
void* malloc(size_t nbytes);
void free(void* ap);

#endif // LIB_H
