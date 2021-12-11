/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
.text

.globl  _start
.type   _start,@function
_start:
    /*
     * Correct stack alignment - that the ABI states that %rsp+8 must be
     * 16-byte aligned.
     *
     * We use the fact that the kernel sets %rdi to wherever auxargs is
     * located and just pass it through unmodified.
     */
    andq    $~15, %rsp
    subq    $8, %rsp
    jmp __start
