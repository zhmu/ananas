/* Terminate the constructors list */
.section .ctors, "aw", @progbits
.align 8
.global __CTOR_LIST_END__
.hidden __CTOR_LIST_END__
__CTOR_LIST_END__:
    .quad   0

.section .dtors, "aw", @progbits
.align 8
    .quad   0
