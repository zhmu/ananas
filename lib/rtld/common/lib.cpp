/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "lib.h"
#include <ananas/statuscode.h>
#include <ananas/syscall-vmops.h>
#include <sys/mman.h> // for PROT_...
#include <sys/stat.h>

extern "C" {
#include <ananas/syscalls.h>
}

namespace
{
    const char* s_ProgName = nullptr;
    char** s_Envp = nullptr;
} // namespace

extern "C" int open(const char* path, int flags, ...)
{
    statuscode_t result = sys_open(path, flags, 0);
    return ananas_statuscode_is_success(result) ? ananas_statuscode_extract_value(result) : -1;
}

extern "C" int close(int fd) { return ananas_statuscode_is_success(sys_close(fd)) ? 0 : -1; }

ssize_t read(int fd, void* buf, size_t count)
{
    statuscode_t result = sys_read(fd, buf, count);
    return ananas_statuscode_is_success(result) ? ananas_statuscode_extract_value(result) : -1;
}

ssize_t write(int fd, const void* buf, size_t count)
{
    statuscode_t result = sys_write(fd, buf, count);
    return ananas_statuscode_is_success(result) ? ananas_statuscode_extract_value(result) : -1;
}

off_t lseek(int fd, off_t offset, int whence)
{
    off_t new_offset = offset;
    statuscode_t result = sys_seek(fd, &new_offset, whence);
    return ananas_statuscode_is_success(result) ? new_offset : -1;
}

int fstat(int fd, struct stat* sb)
{
    return ananas_statuscode_is_success(sys_fstat(fd, sb)) ? 0 : -1;
}

void exit(int status) { sys_exit(status); }

inline int prot_to_flags(int prot)
{
    int flags = 0;
    if (prot & PROT_READ)
        flags |= VMOP_FLAG_READ;
    if (prot & PROT_WRITE)
        flags |= VMOP_FLAG_WRITE;
    if (prot & PROT_EXEC)
        flags |= VMOP_FLAG_EXECUTE;
    return flags;
}

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    struct VMOP_OPTIONS vo;

    memset(&vo, 0, sizeof(vo));
    vo.vo_size = sizeof(vo);
    vo.vo_op = OP_MAP;
    vo.vo_addr = addr;
    vo.vo_len = length;
    vo.vo_flags = prot_to_flags(prot);

    if (flags & MAP_PRIVATE)
        vo.vo_flags |= VMOP_FLAG_PRIVATE;
    else
        vo.vo_flags |= VMOP_FLAG_SHARED;
    if (flags & MAP_FIXED)
        vo.vo_flags |= VMOP_FLAG_FIXED;

    if (fd != -1) {
        vo.vo_flags |= VMOP_FLAG_FD;
        vo.vo_fd = fd;
    }
    vo.vo_offset = offset;

    return ananas_statuscode_is_success(sys_vmop(&vo)) ? static_cast<void*>(vo.vo_addr)
                                                       : reinterpret_cast<void*>(-1);
}

int munmap(void* addr, size_t length)
{
    struct VMOP_OPTIONS vo;

    memset(&vo, 0, sizeof(vo));
    vo.vo_size = sizeof(vo);
    vo.vo_op = OP_UNMAP;
    vo.vo_addr = addr;
    vo.vo_len = length;

    return ananas_statuscode_is_success(sys_vmop(&vo)) ? 0 : -1;
}

int mprotect(void* addr, size_t len, int prot)
{
    struct VMOP_OPTIONS vo;

    memset(&vo, 0, sizeof(vo));
    vo.vo_size = sizeof(vo);
    vo.vo_op = OP_CHANGE_ACCESS;
    vo.vo_addr = addr;
    vo.vo_len = len;
    vo.vo_flags = prot_to_flags(prot);

    return ananas_statuscode_is_success(sys_vmop(&vo)) ? 0 : -1;
}

size_t strlen(const char* s)
{
    size_t n = 0;
    while (*s++ != '\0')
        n++;
    return n;
}

char* strcpy(char* d, const char* s)
{
    memcpy(d, s, strlen(s) + 1 /* terminating \0 */);
    return d;
}

char* strncpy(char* d, const char* s, size_t n)
{
    size_t i = 0;
    for (/* nothing */; s[i] != '\0' && i < n; i++)
        d[i] = s[i];

    for (/* nothing */; i < n; i++)
        d[i] = '\0';
    return d;
}

char* strchr(const char* s, int c)
{
    for (/* nothing */; *s != '\0'; s++)
        if (*s == c)
            return const_cast<char*>(s);
    return nullptr;
}

char* strcat(char* dest, const char* src)
{
    char* d = dest;
    d += strlen(d);
    while ((*d++ = *src++) != '\0')
        /* nothing */;
    return dest;
}

int strcmp(const char* a, const char* b)
{
    while (*a != '\0' && *a == *b)
        a++, b++;
    return *a - *b;
}

int strncmp(const char* s1, const char* s2, size_t n)
{
    while (n > 0 && *s1 != '\0' && *s2 != '\0' && *s1 == *s2) {
        n--;
        s1++;
        s2++;
    }
    return (n == 0) ? n : *s1 - *s2;
}

char* strdup(const char* s)
{
    size_t len = strlen(s) + 1;
    auto p = static_cast<char*>(malloc(len));
    if (p != nullptr)
        memcpy(p, s, len);
    return p;
}

void* memset(void* b, int c, size_t len)
{
    void* d = b;

    /* Sets sz bytes of variable type T-sized data */
#define DO_SET(T, sz, v)                                               \
    do {                                                               \
        size_t x = (size_t)sz;                                         \
        while (x >= sizeof(T)) {                                       \
            *(T*)d = (T)v;                                             \
            x -= sizeof(T);                                            \
            d = static_cast<void*>(static_cast<char*>(d) + sizeof(T)); \
            len -= sizeof(T);                                          \
        }                                                              \
    } while (0)

    /* First of all, attempt to align to 32-bit boundary */
    if (len >= 4 && ((addr_t)b & 3))
        DO_SET(uint8_t, len & 3, c);

    /* Attempt to set everything using 4 bytes at a time */
    DO_SET(uint32_t, len, ((uint32_t)c << 24 | (uint32_t)c << 16 | (uint32_t)c << 8 | (uint32_t)c));

    /* Handle the leftovers */
    DO_SET(uint8_t, len, c);

#undef DO_SET
    return d;
}

void memcpy(void* dst, const void* src, size_t len)
{
    /* Copies sz bytes of variable type T-sized data */
#define DO_COPY(T, sz)           \
    do {                         \
        size_t x = (size_t)sz;   \
        while (x >= sizeof(T)) { \
            *(T*)d = *(T*)s;     \
            x -= sizeof(T);      \
            s += sizeof(T);      \
            d += sizeof(T);      \
            len -= sizeof(T);    \
        }                        \
    } while (0)

    auto d = static_cast<char*>(dst);
    auto s = static_cast<const char*>(src);

    /* First of all, attempt to align to 32-bit boundary */
    if (len >= 4 && ((addr_t)dst & 3))
        DO_COPY(uint8_t, len & 3);

    /* Attempt to copy everything using 4 bytes at a time */
    DO_COPY(uint32_t, len);

    /* Cover the leftovers */
    DO_COPY(uint8_t, len);

#undef DO_COPY
}

void panic(const char* msg)
{
    write(STDERR_FILENO, msg, strlen(msg));
    exit(-1);
}

void* operator new(size_t len) throw() { return malloc(len); }

void operator delete(void* p) throw() { free(p); }

void InitializeFromStack(register_t* stk)
{
    auto argc = *stk++;
    s_ProgName = reinterpret_cast<char*>(*stk);
    while (argc--)
        stk++; // skip argc
    stk++;     // skip 0
    s_Envp = reinterpret_cast<char**>(stk);
}

const char* GetProgName() { return s_ProgName; }

char* getenv(const char* name)
{
    char** ptr = s_Envp;
    while (*ptr != nullptr) {
        char* p = strchr(*ptr, '=');
        if (p == nullptr)
            return nullptr; // corrupt environment
        if (strncmp(*ptr, name, p - *ptr) == 0)
            return p + 1;

        ptr++;
    }

    return nullptr;
}
