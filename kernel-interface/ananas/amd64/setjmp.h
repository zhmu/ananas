/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __AMD64_SETJMP_H__
#define __AMD64_SETJMP_H__

typedef struct {
    long addr;
    long rbx;
    long rsp;
    long rbp;
    long r12;
    long r13;
    long r14;
    long r15;
} jmp_buf[1];

#endif /* __AMD64_SETJMP_H__ */
