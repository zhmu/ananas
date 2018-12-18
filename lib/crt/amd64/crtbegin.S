/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
.section .ctors, "aw", @progbits
.align 8
__CTOR_LIST__:
    .quad   -1

.section .dtors, "aw", @progbits
.align 8
__DTOR_LIST__:
    .quad   -1

.section .data.rel, "aw", @progbits
.align 8

/*
 * Provide the __dso_handle symbol - this is used to identify the instance of the
 * shared library and only execute destructors belonging to it.
 */
.type   __dso_handle, @object
.size   __dso_handle, 8
.globl  __dso_handle
.hidden __dso_handle
__dso_handle:
#ifdef SHARED
    .quad   __dso_handle
#else
    .quad   0
#endif

__ran_ctors:
    .zero   1

__ran_dtors:
    .zero   1


.text

__run_global_ctors:
    /* Ensure we don't run the constructors twice */
    cmpb    $0, __ran_ctors(%rip)
    jz  1f

    ret

1:  incb    __ran_ctors(%rip)

    /*
     * Run all items in the .ctors; note that we move from the back towards
     * the front, yet we skip the last item.
     *
     * We use %rbx as the ABI specifies that this needs to be saved across
     * function calls (which we faithfully do)
     */
    pushq   %rbx
    leaq    __CTOR_LIST_END__(%rip), %rbx
    jmp 4f

3:  movq    (%rbx), %rax        /* Fetch pointer from ctor list */
    cmpq    $-1, %rax       /* Is it -1, the end-marker? */
    je  2f          /* Yes, stop */

    callq   *%rax
4:  subq    $8, %rbx
    jmp 3b

2:  popq    %rbx
    ret


/* Runs all .dtors entries in order */
__run_global_dtors:
    /* Ensure we don't run the destructors twice */
    cmpb    $0, __ran_dtors(%rip)
    jz  1f

    ret

1:  incb    __ran_dtors(%rip)

#if 0 // NOT YET
#ifdef SHARED
    cmpq    $0, __cxa_finalize@GOTPCREL(%rip)
    je  5f

    /* Call destructors of global (or local static) C++ objects and exit functions */
    movq    __dso_handle(%rip), %rdi
    call    __c
#endifxa_finalize@PLT

5:
#endif
#endif

    /*
     * For all pointers in the .dtors, run them until we hit NULL. Note
     * that we must skip the first entry here.
     *
     * We use %rbx as the ABI specifies that this needs to be saved across
     * function calls (which we faithfully do)
     */
    pushq   %rbx
    leaq    __DTOR_LIST__(%rip), %rbx
    jmp 4f

3:
    movq    (%rbx), %rax        /* Fetch pointer from dtor list */
    testq   %rax, %rax      /* Zero? */
    jz  2f          /* Yes, enough - stop */

    /* Run the dtor function and continue */
    call    *%rax
4:  addq    $8, %rbx
    jmp 3b


2:  popq    %rbx
    ret

/*
 * Now place our __run_global_[cd]tors function in .init/.fini sections so that
 * we'll run all {con,de}structors upon enter/leaving.
 */
.section .init, "ax", @progbits
    call    __run_global_ctors
.section .fini, "ax", @progbits
    call    __run_global_dtors
