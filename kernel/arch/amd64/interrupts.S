/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
/*
 * Low-level assembly code to pass an interrupt to a higher-level handler.
 */
.text
.globl exception0, exception1, exception2, exception3, exception4, exception5
.globl exception6, exception7, exception8, exception9, exception10, exception11
.globl exception12, exception13, exception14, exception16, exception17
.globl exception18, exception19
.globl syscall_handler
.globl thread_trampoline
.globl lapic_irq_range_1, lapic_irq_range_2, lapic_irq_range_3, lapic_irq_range_4
.globl lapic_irq_range_5, lapic_irq_range_6, lapic_irq_range_7

#include "kernel-md/vm.h"
#include "kernel-md/thread.h"
#include "kernel-md/lapic.h"
#include "kernel-md/smp.h"
#include "options.h"
#include "asmsyms.h"

#define THREAD_FLAG_SIGPENDING 0x80 // XXX this is in thread.h

#define SAVE_REGISTERS \
    movq    %rax, SF_RAX(%rsp); \
    movq    %rbx, SF_RBX(%rsp); \
    movq    %rcx, SF_RCX(%rsp); \
    movq    %rdx, SF_RDX(%rsp); \
    movq    %rbp, SF_RBP(%rsp); \
    movq    %rsi, SF_RSI(%rsp); \
    movq    %rdi, SF_RDI(%rsp); \
    /* movq %rsp, SF_RSP(%rsp); */ \
    movq    %r8,  SF_R8 (%rsp); \
    movq    %r9,  SF_R9 (%rsp); \
    movq    %r10, SF_R10(%rsp); \
    movq    %r11, SF_R11(%rsp); \
    movq    %r12, SF_R12(%rsp); \
    movq    %r13, SF_R13(%rsp); \
    movq    %r14, SF_R14(%rsp); \
    movq    %r15, SF_R15(%rsp); \
    leaq    SF_FPU(%rsp), %rax; \
    fxsave  (%rax); \
    movw    %ds, %ax; \
    movw    %ax, SF_DS(%rsp); \
    movw    %es, %ax; \
    movw    %ax, SF_ES(%rsp); \

#define RESTORE_REGISTERS \
    leaq    SF_FPU(%rsp), %rax; \
    fxrstor (%rax); \
    movq    SF_RAX(%rsp), %rax; \
    movq    SF_RBX(%rsp), %rbx; \
    movq    SF_RCX(%rsp), %rcx; \
    movq    SF_RDX(%rsp), %rdx; \
    movq    SF_RBP(%rsp), %rbp; \
    movq    SF_RSI(%rsp), %rsi; \
    movq    SF_RDI(%rsp), %rdi; \
    /* movq SF_RSP(%rsp), %rsp; */ \
    movq    SF_R8 (%rsp), %r8; \
    movq    SF_R9 (%rsp), %r9; \
    movq    SF_R10(%rsp), %r10; \
    movq    SF_R11(%rsp), %r11; \
    movq    SF_R12(%rsp), %r12; \
    movq    SF_R13(%rsp), %r13; \
    movq    SF_R14(%rsp), %r14; \
    movq    SF_R15(%rsp), %r15

#define EXCEPTION_WITHOUT_ERRCODE(n) \
exception ## n: \
    subq    $SF_RIP, %rsp; \
    movq    $n, SF_TRAPNO(%rsp); \
    movq    $0, SF_ERRNUM(%rsp); \
    jmp do_exception

#define EXCEPTION_WITH_ERRCODE(n) \
exception ## n: \
    subq    $SF_ERRNUM, %rsp; \
    movq    $n, SF_TRAPNO(%rsp); \
    jmp do_exception

#define IRQ_HANDLER(n) \
irq ## n: \
    subq    $SF_RIP, %rsp; \
    movq    $n, SF_TRAPNO(%rsp); \
    movq    $0, SF_ERRNUM(%rsp); \
    jmp do_irq

/*
 * We only have 7 APIC_ISR_HANDLER's; each will service a block of 32
 * interrupts (the first would correspond to vector 0..31 and these are
 * reserved for exceptions, so we directly handle them using EXCEPTION...).
 *
 * The reason for this check is that it  prevents having a lot of extra code,
 * as we can simply check the LAPIC ISR register for the first set bit and
 * dispatch that interrupt.
 */
#define APIC_IRQ_RANGE_HANDLER(n) \
lapic_irq_range_ ## n: \
    subq    $SF_RIP, %rsp; \
    SAVE_REGISTERS \
    \
    /* If we didn't come from the kernel, swap the %gs register */ \
    cmpl    $GDT_SEL_KERNEL_CODE, SF_CS(%rsp); \
    je  1f; \
    \
    swapgs; \
    \
1:  /* Increment the nested IRQ count */ \
    incl    %gs:(PCPU_NESTEDIRQ); \
    \
    /* Restore the interrupt flag */ \
    movq    SF_RFLAGS(%rsp), %rax; \
    testq   $0x200, %rax; \
    jz  1f; \
    \
    sti; \
    \
1:  movq    lapic_base, %rdx; \
    /* Read ISR field; */ \
    movl    (LAPIC_ISR + 16 * n)(%rdx), %eax; \
    bsrl    %eax, %eax; \
    jz  lapic_no_irq_pending; \
    \
    /* Call the interrupt handler! */ \
    addq    $(32 * n), %rax; \
    movq    %rax, SF_TRAPNO(%rsp); \
    movq    %rsp, %rdi; \
    call    lapic_interrupt_handler; \
    jmp do_return_check_sig

lapic_no_irq_pending:
    decl    %gs:(PCPU_NESTEDIRQ)
    jmp do_return_check_sig

do_exception:
    SAVE_REGISTERS

    /* If we didn't come from the kernel, swap the %gs register */
    cmpl    $GDT_SEL_KERNEL_CODE, SF_CS(%rsp)
    je  1f

    swapgs

1:

    /* Call the exception handler! */
    movq    %rsp, %rdi
    call    exception

do_return_check_sig:
    movq    %gs:(PCPU_CURTHREAD), %rax
    testl   $THREAD_FLAG_SIGPENDING, T_FLAGS(%rax)
    jz  do_return

    /*
     * If we are not returning to userland, skip trying to handle signals;
     * we must only change the stackframe on a kernel->user transition.
     */
    cmpl    $GDT_SEL_KERNEL_CODE, SF_CS(%rsp)
    je  do_return

do_return_signal:
    /*
     * Enable interrupts here - the signal code may grab locks. This is safe,
     * as we only get here during a transition to userland so any nested
     * interrupts do not end up here.
     */
    sti
    movq    %rsp, %rdi
    call    md_invoke_signal
    orq %rax, %rax
    jz  do_return

    movq    %rax, %rsp

do_return:
    /* Restore %gs if needed */
    cmpl    $GDT_SEL_KERNEL_CODE, SF_CS(%rsp)
    je  1f

    swapgs

1:
    RESTORE_REGISTERS

    /* skip over the stackframe */
    addq    $SF_RIP, %rsp
    iretq

do_irq:
    SAVE_REGISTERS

    /* If we didn't come from the kernel, swap the %gs register */
    cmpl    $GDT_SEL_KERNEL_CODE, SF_CS(%rsp)
    je  1f

    swapgs

1:  /* Increment the nested IRQ count */
    incl    %gs:(PCPU_NESTEDIRQ)

    /* Restore the interrupt flag */
    movq    SF_RFLAGS(%rsp), %rax
    testq   $0x200, %rax
    jz  1f

    sti

1:  /* Call the interrupt handler! */
    movq    %rsp, %rdi
    call    interrupt_handler

    jmp do_return_check_sig

/* Now we just need to list the exception handlers */
EXCEPTION_WITHOUT_ERRCODE(0)
EXCEPTION_WITHOUT_ERRCODE(1)
EXCEPTION_WITHOUT_ERRCODE(2) /* NMI */
EXCEPTION_WITHOUT_ERRCODE(3)
EXCEPTION_WITHOUT_ERRCODE(4)
EXCEPTION_WITHOUT_ERRCODE(5)
EXCEPTION_WITHOUT_ERRCODE(6)
EXCEPTION_WITHOUT_ERRCODE(7)
EXCEPTION_WITH_ERRCODE(8)
EXCEPTION_WITHOUT_ERRCODE(9)
EXCEPTION_WITH_ERRCODE(10)
EXCEPTION_WITH_ERRCODE(11)
EXCEPTION_WITH_ERRCODE(12)
EXCEPTION_WITH_ERRCODE(13)
EXCEPTION_WITH_ERRCODE(14)
EXCEPTION_WITHOUT_ERRCODE(16)
EXCEPTION_WITH_ERRCODE(17)
EXCEPTION_WITHOUT_ERRCODE(18)
EXCEPTION_WITHOUT_ERRCODE(19)

/* And the APIC interrupts handlers */
APIC_IRQ_RANGE_HANDLER(1)
APIC_IRQ_RANGE_HANDLER(2)
APIC_IRQ_RANGE_HANDLER(3)
APIC_IRQ_RANGE_HANDLER(4)
APIC_IRQ_RANGE_HANDLER(5)
APIC_IRQ_RANGE_HANDLER(6)
APIC_IRQ_RANGE_HANDLER(7)

.globl  irq_spurious, ipi_periodic, ipi_panic
irq_spurious:
    iretq

ipi_periodic:
    IRQ_HANDLER(SMP_IPI_PERIODIC)

ipi_panic:
    IRQ_HANDLER(SMP_IPI_PANIC)

/*
 * System call handler - will be called using SYSCALL; only %cs/%ss will be set
 * up. We use the same calling convention as Linux, as outlined in the System V
 * ABI AMD64 specification 0.99, section A.2.1.
 *
 * On syscall, %rcx is set to the userland %rip and %r11 are the original flags.
 */
syscall_handler:
    swapgs

    /* Store the userland's %rsp in PCPU register and switch to the kernel %rsp */
    movq    %rsp, %gs:PCPU_SYSCALLRSP
    movq    %gs:PCPU_RSP0, %rsp

    /* Create a stack frame and store the syscall arguments */
    subq    $SF_SIZE, %rsp
    movq    %r11, SF_RFLAGS(%rsp)   /* flags were in %r11 */
    movq    %rcx, SF_RIP(%rsp)  /* rip was in %rcx */
    movq    %gs:PCPU_SYSCALLRSP, %rcx
    movq    %rcx, SF_RSP(%rsp)
    /* Syscall arguments */
    movq    %rax, SF_RAX(%rsp)
    movq    %rdi, SF_RDI(%rsp)
    movq    %rsi, SF_RSI(%rsp)
    movq    %rdx, SF_RDX(%rsp)
    movq    %r10, SF_R10(%rsp)
    movq    %r8, SF_R8(%rsp)
    movq    %r9, SF_R9(%rsp)
    /* Registers we need to save */
    movq    %rbx, SF_RBX(%rsp)
    movq    %rbp, SF_RBP(%rsp)
    movq    %r12, SF_R12(%rsp)
    movq    %r13, SF_R13(%rsp)
    movq    %r14, SF_R14(%rsp)
    movq    %r15, SF_R15(%rsp)

    /* Re-enable interrupts; they were always enabled coming from user mode */
    sti

    movq    %rsp, %rdi
    call    amd64_syscall

syscall_return:
    /*
     * Note that we don't actually need to restore rbx, rbp, r12-r15 as
     * syscall() is a plain C function and thus will do that for us.
     */
    cli

    /* If we need to restore the entire context, do so */
    movq    %gs:(PCPU_CURTHREAD), %rax
    testl   $THREAD_MDFLAG_FULLRESTORE, T_MDFLAGS(%rax)
    jz  1f

    /*
     * Do a full register restore - this only happens after things like exec()
     * so we do not need to check for pending signal here
     */
    andl    $~THREAD_MDFLAG_FULLRESTORE, T_MDFLAGS(%rax)
    jmp do_return

1:  /* If we need to invoke a signal handler, do so... */
    testl   $THREAD_FLAG_SIGPENDING, T_FLAGS(%rax)
    jnz do_return_signal

    movq    SF_RAX(%rsp), %rax  /* return value */
    movq    SF_RFLAGS(%rsp), %r11   /* original rflags */
    movq    SF_RIP(%rsp), %rcx  /* original %rip */
    movq    SF_RSP(%rsp), %rsp  /* original %rsp */
    swapgs
    sysretq

thread_trampoline:
    /* release our previous thread (will be in %rbx, see md_thread_switch) */
    movq    %rbx, %rdi
    call    scheduler_release

    jmp do_return
