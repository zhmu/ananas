/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
/*
 * Init functionality; largely inspired by NetBSD's lib/csu/common/crt0-common.c.
 */
#include <stdlib.h>
#include <stdint.h>

extern void _libc_init(int argc, char** argv, char** envp, char** auxv);
extern int main(int argc, char** argv, char** envp);
extern void exit(int);
extern char** environ;

extern void* _DYNAMIC;
#pragma weak _DYNAMIC

void __start(unsigned long* stk, void (*cleanup)() /* provided by rtld-elf */)
{
    int argc = *stk++;
    char** argv = (char**)stk;
    stk += argc + 1 /* terminating null */;
    environ = (char**)stk++;
    void* auxv = stk;
    _libc_init(argc, argv, environ, auxv);
    if (&_DYNAMIC != NULL)
        atexit(cleanup);

    exit(main(argc, argv, environ));
}
