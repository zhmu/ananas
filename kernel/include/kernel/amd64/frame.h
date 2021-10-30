/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "kernel/types.h"

struct STACKFRAME {
    uint64_t sf_trapno;
    uint64_t sf_rax;
    uint64_t sf_rbx;
    uint64_t sf_rcx;
    uint64_t sf_rdx;
    uint64_t sf_rbp;
    uint64_t sf_rsi;
    uint64_t sf_rdi;
    uint64_t sf_r8;
    uint64_t sf_r9;
    uint64_t sf_r10;
    uint64_t sf_r11;
    uint64_t sf_r12;
    uint64_t sf_r13;
    uint64_t sf_r14;
    uint64_t sf_r15;
    uint16_t sf_ds;
    uint16_t sf_es;
    /* Set by the hardware */
    register_t sf_errnum;
    register_t sf_rip;
    register_t sf_cs;
    register_t sf_rflags;
    register_t sf_rsp;
    register_t sf_ss;
};
