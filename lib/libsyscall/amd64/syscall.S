/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
/* Note that we must store %rcx because it will hold the return %rip */
#define SYSCALL(x)      \
    movq    $x, %rax;   \
    movq    %rcx, %r10; \
    syscall;        \
    ret

.text

#include "syscall.gen.S"
