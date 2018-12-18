#include "../common/note.S"

/* Start of the initialization functions */
.section .init,"ax",@progbits
.align 16
.globl  _init
.type   _init,@function
_init:
    subq    $8, %rsp

/* Start of the finalization functions */
.section .fini,"ax",@progbits
.align 16
.globl  _fini
.type   _fini,@function
_fini:
    subq    $8, %rsp
