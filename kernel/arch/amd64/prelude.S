// Terminate the .ctors list witrh a -1 entry
.section .ctors, "aw", @progbits
.align 8
    .quad   -1

.text

.globl __run_global_ctors
.type __run_global_ctors,@function
__run_global_ctors:
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
