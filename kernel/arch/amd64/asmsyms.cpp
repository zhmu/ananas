/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/asmsymbols.h"
#include "kernel/pcpu.h"
#include "kernel/thread.h"
#include "kernel/vmspace.h"
#include "kernel-md/asmsyms.h"
#include "kernel-md/frame.h"
#include "kernel-md/pcpu.h"
#include "kernel-md/smp.h"
#include "kernel-md/thread.h"
#include "../sys/syscall.h"

#pragma GCC diagnostic ignored "-Winvalid-offsetof"

// Verify that whatever is in asmsyms.h matches with reality
static_assert(SF_TRAPNO == offsetof(struct STACKFRAME, sf_trapno));
static_assert(SF_RAX == offsetof(struct STACKFRAME, sf_rax));
static_assert(SF_RBX == offsetof(struct STACKFRAME, sf_rbx));
static_assert(SF_RCX == offsetof(struct STACKFRAME, sf_rcx));
static_assert(SF_RDX == offsetof(struct STACKFRAME, sf_rdx));
static_assert(SF_RBP == offsetof(struct STACKFRAME, sf_rbp));
static_assert(SF_RSI == offsetof(struct STACKFRAME, sf_rsi));
static_assert(SF_RDI == offsetof(struct STACKFRAME, sf_rdi));
static_assert(SF_RSP == offsetof(struct STACKFRAME, sf_rsp));
static_assert(SF_R8 == offsetof(struct STACKFRAME, sf_r8));
static_assert(SF_R9 == offsetof(struct STACKFRAME, sf_r9));
static_assert(SF_R10 == offsetof(struct STACKFRAME, sf_r10));
static_assert(SF_R11 == offsetof(struct STACKFRAME, sf_r11));
static_assert(SF_R12 == offsetof(struct STACKFRAME, sf_r12));
static_assert(SF_R13 == offsetof(struct STACKFRAME, sf_r13));
static_assert(SF_R14 == offsetof(struct STACKFRAME, sf_r14));
static_assert(SF_R15 == offsetof(struct STACKFRAME, sf_r15));
static_assert(SF_ERRNUM == offsetof(struct STACKFRAME, sf_errnum));
static_assert(SF_RFLAGS == offsetof(struct STACKFRAME, sf_rflags));
static_assert(SF_RIP == offsetof(struct STACKFRAME, sf_rip));
static_assert(SF_CS == offsetof(struct STACKFRAME, sf_cs));
static_assert(SF_DS == offsetof(struct STACKFRAME, sf_ds));
static_assert(SF_ES == offsetof(struct STACKFRAME, sf_es));
static_assert(SF_SS == offsetof(struct STACKFRAME, sf_ss));
static_assert(SF_SIZE == sizeof(struct STACKFRAME));

static_assert(PCPU_CURTHREAD == offsetof(struct PCPU, curthread));
static_assert(PCPU_NESTEDIRQ == offsetof(struct PCPU, nested_irq));
static_assert(PCPU_SYSCALLRSP == offsetof(struct PCPU, syscall_rsp));
static_assert(PCPU_RSP0 == offsetof(struct PCPU, rsp0));

static_assert(SMP_CPU_SIZE == sizeof(struct X86_CPU));
static_assert(SMP_CPU_LAPICID == offsetof(struct X86_CPU, cpu_lapic_id));
static_assert(SMP_CPU_STACK == offsetof(struct X86_CPU, cpu_stack));

static_assert(T_SIG_PENDING == offsetof(Thread, t_sig_pending));
static_assert(T_MDFLAGS == offsetof(Thread, t_md_flags));

static_assert(VMSPACE_MD_PAGEDIR == offsetof(VMSpace, vs_md_pagedir));

// These must correspond with syscall_handler() in interrupts.S
static_assert(SYSARG_NUM == offsetof(struct SYSCALL_ARGS, number));
static_assert(SYSARG_ARG1 == offsetof(struct SYSCALL_ARGS, arg1));
static_assert(SYSARG_ARG2 == offsetof(struct SYSCALL_ARGS, arg2));
static_assert(SYSARG_ARG3 == offsetof(struct SYSCALL_ARGS, arg3));
static_assert(SYSARG_ARG4 == offsetof(struct SYSCALL_ARGS, arg4));
static_assert(SYSARG_ARG5 == offsetof(struct SYSCALL_ARGS, arg5));
static_assert(SYSARG_SIZE == sizeof(struct SYSCALL_ARGS));
